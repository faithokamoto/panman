#include <iostream>
#include <string>
#include <vector>
#include <tbb/parallel_invoke.h> 
#include <tbb/parallel_for_each.h>

#include "PangenomeMAT.hpp"

PangenomeMAT::Node::Node(std::string id, float len){
	identifier = id;
    level = 1;
	branchLength = len;
	parent = NULL;
}

PangenomeMAT::Node::Node(std::string id, Node* par, float len){
	identifier = id;
    branchLength = len;
	parent = par;
    level = par->level + 1;
    par->children.push_back(this);
}

void PangenomeMAT::stringSplit (std::string const& s, char delim, std::vector<std::string>& words) {
    // TIMEIT();
    size_t start_pos = 0, end_pos = 0;
    while ((end_pos = s.find(delim, start_pos)) != std::string::npos) {
        // if ((end_pos == start_pos) || end_pos >= s.length()) {
        if (end_pos >= s.length()) {
            break;
        }
        words.emplace_back(s.substr(start_pos, end_pos-start_pos));
        start_pos = end_pos+1;
    }
    auto last = s.substr(start_pos, s.size()-start_pos);
    if (last != "") {
        words.push_back(std::move(last));
    }
}

PangenomeMAT::Node* PangenomeMAT::Tree::createTreeFromNewickString(std::string newickString) {

    PangenomeMAT::Node* newTreeRoot;

    std::vector<std::string> leaves;
    std::vector<size_t> numOpen;
    std::vector<size_t> numClose;
    std::vector<std::queue<float>> branchLen (128);  // will be resized later if needed
    size_t level = 0;

    std::vector<std::string> s1;
    stringSplit(newickString, ',', s1);

    numOpen.reserve(s1.size());
    numClose.reserve(s1.size());

    for (auto s: s1) {
        size_t no = 0;
        size_t nc = 0;
        size_t leafDepth = 0;

        bool stop = false;
        bool branchStart = false;
        std::string leaf = "";
        std::string branch = "";

        for (auto c: s) {
            if (c == ':') {
                stop = true;
                branch = "";
                branchStart = true;
            } else if (c == '(') {
                no++;
                level++;
                if (branchLen.size() <= level) {
                    branchLen.resize(level*2);
                }
            } else if (c == ')') {
                stop = true;
                nc++;
                float len = (branch.size() > 0) ? std::stof(branch) : -1.0;
                branchLen[level].push(len);
                level--;
                branchStart = false;
            } else if (!stop) {
                leaf += c;
                branchStart = false;
                leafDepth = level;

            } else if (branchStart) {
                if (isdigit(c)  || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+') {
                    branch += c;
                }
            }
        }
        leaves.push_back(std::move(leaf));
        numOpen.push_back(no);
        numClose.push_back(nc);
        float len = (branch.size() > 0) ? std::stof(branch) : -1.0;
        branchLen[level].push(len);

        // Adjusting max and mean depths
        maxDepth = std::max(maxDepth, leafDepth);
        meanDepth += leafDepth;

    }

    meanDepth /= leaves.size();

    if (level != 0) {
        fprintf(stderr, "ERROR: incorrect Newick format!\n");
        exit(1);
    }

    numLeaves = leaves.size();

    std::stack<Node*> parentStack;

    for (size_t i=0; i<leaves.size(); i++) {
        auto leaf = leaves[i];
        auto no = numOpen[i];
        auto nc = numClose[i];
        for (size_t j=0; j<no; j++) {
            std::string nid = newInternalNodeId();
            Node* newNode = NULL;
            if (parentStack.size() == 0) {
                newNode = new Node(nid, branchLen[level].front());
                newTreeRoot = newNode;
            } else {
                newNode = new Node(nid, parentStack.top(), branchLen[level].front());
            }
            branchLen[level].pop();
            level++;

            allNodes[nid] = newNode;
            parentStack.push(newNode);
        }
        Node* leafNode = new Node(leaf, parentStack.top(), branchLen[level].front());
        parentStack.top()->children.push_back(leafNode);

        allNodes[leaf] = leafNode;

        branchLen[level].pop();
        for (size_t j=0; j<nc; j++) {
            parentStack.pop();
            level--;
        }
    }

    if (newTreeRoot == NULL) {
        fprintf(stderr, "WARNING: Tree found empty!\n");
    }

    return newTreeRoot;
}

void PangenomeMAT::Tree::assignMutationsToNodes(Node* root, size_t currentIndex, std::vector< MAT::node >& nodes){
    std::vector< PangenomeMAT::NucMut > storedNucMutation;
    for(int i = 0; i < nodes[currentIndex].nuc_mutation_size(); i++){
        storedNucMutation.push_back( PangenomeMAT::NucMut(nodes[currentIndex].nuc_mutation(i)) );
    }

    PangenomeMAT::BlockMut storedBlockMutation;
    storedBlockMutation.loadFromProtobuf(nodes[currentIndex].block_mutation());

    root->nucMutation = storedNucMutation;
    root->blockMutation = storedBlockMutation;

    for(auto child: root->children){
        assignMutationsToNodes(child, currentIndex+1, nodes);
    }

}

PangenomeMAT::Tree::Tree(std::ifstream& fin){

    currInternalNode = 0;
	numLeaves = 0;
    maxDepth = 0;
    meanDepth = 0;

    MAT::tree mainTree;

	if(!mainTree.ParseFromIstream(&fin)){
		std::cerr << "Could not read tree from input file." << std::endl;
		return;
	}

	root = createTreeFromNewickString(mainTree.newick());

    std::vector< MAT::node > storedNodes;
    for(int i = 0; i < mainTree.nodes_size(); i++){
        storedNodes.push_back(mainTree.nodes(i));
    }

    assignMutationsToNodes(root, 0, storedNodes);
}

int PangenomeMAT::Tree::getTotalParsimonyParallel(PangenomeMAT::MutationType type){
    int totalMutations = 0;

    std::queue<Node *> bfsQueue;

    bfsQueue.push(root);
    while(!bfsQueue.empty()){
        Node* current = bfsQueue.front();
        bfsQueue.pop();

        // Process children of current node
        for(auto nucMutation: current->nucMutation){
            if((nucMutation.condensed & 0x3) == type){
                totalMutations++;
            }
        }

        for(auto child: current->children){
            bfsQueue.push(child);
        }
    }

    return totalMutations;
}

int PangenomeMAT::Tree::getTotalParsimony(PangenomeMAT::MutationType type){
    int totalMutations = 0;

    std::queue<Node *> bfsQueue;

    bfsQueue.push(root);
    while(!bfsQueue.empty()){
        Node* current = bfsQueue.front();
        bfsQueue.pop();

        // Process children of current node
        for(auto nucMutation: current->nucMutation){
            if((nucMutation.condensed & 0x3) == type){
                totalMutations++;
            }
        }

        for(auto child: current->children){
            bfsQueue.push(child);
        }
    }

    return totalMutations;
}

void PangenomeMAT::Tree::printSummary(){
    // Traversal test
    std::cout << "Total Nodes in Tree: " << currInternalNode + numLeaves << std::endl;
    std::cout << "Total Samples in Tree: " << numLeaves << std::endl;
    std::cout << "Total Substitutions: " << getTotalParsimony(PangenomeMAT::MutationType::S) << std::endl;
    std::cout << "Total Insertions: " << getTotalParsimony(PangenomeMAT::MutationType::I) << std::endl;
    std::cout << "Total Deletions: " << getTotalParsimony(PangenomeMAT::MutationType::D) << std::endl;
    std::cout << "Total SNP mutations: " << getTotalParsimony(PangenomeMAT::MutationType::SNP) << std::endl;
    std::cout << "Max Tree Depth: " << maxDepth << std::endl;
    std::cout << "Mean Tree Depth: " << meanDepth << std::endl;
}

void PangenomeMAT::Tree::printBfs(){
    // Traversal test
    std::queue<Node *> bfsQueue;
    int prevLev = 0;
    bfsQueue.push(root);
    while(!bfsQueue.empty()){
        Node* current = bfsQueue.front();
        bfsQueue.pop();

        if(current->level != prevLev){
            std::cout << '\n';
            prevLev = current->level;
        }
        std::cout << '(' << current->identifier << "," << current->branchLength << ") ";

        for(auto child: current->children){
            bfsQueue.push(child);
        }
    }
}

int main(int argc, char* argv[]){
	std::ifstream input(argv[1]);

	PangenomeMAT::Tree T(input);
    // T.printBfs();
    T.printSummary();
}