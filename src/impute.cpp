
#include "panmanUtils.hpp"

const int32_t MAX_SNV_IMPUTE_DIST = std::numeric_limits<int32_t>::max();
const int32_t MAX_INSERTION_IMPUTE_DIST = std::numeric_limits<int32_t>::max();

void panmanUtils::Tree::imputeNs() {
    std::cout << "Imputing a tree" << std::endl;

    std::unordered_map< panmanUtils::Node*, std::unordered_set< panmanUtils::SNVPosition > > snvs;
    std::unordered_map< panmanUtils::Node*, std::unordered_set< panmanUtils::IndelPosition > > insertions;
    findMutationsToN(root, snvs, insertions);

    for (const auto toImpute: snvs) {
        for (const auto& curMut: toImpute.second) {
            imputeSNV(toImpute.first, curMut, nullptr, 0, snvs);
        }
    }
    
    for (const auto& toImpute: insertions) {
        for (const auto& curMut: toImpute.second) {
            imputeInsertion(toImpute.first, curMut, nullptr, 0, insertions);
        }
    }
}

const void panmanUtils::Tree::findMutationsToN(panmanUtils::Node* node, 
        std::unordered_map< panmanUtils::Node*, std::unordered_set< panmanUtils::SNVPosition > >& snvs,
        std::unordered_map< panmanUtils::Node*, std::unordered_set< panmanUtils::IndelPosition > >& insertions) {
    if (node == nullptr) return;

    snvs[node] = std::unordered_set< panmanUtils::SNVPosition >();
    // Store insertions specially so they can easily be combined
    std::vector< panmanUtils::IndelPosition > insertionsSoFar;
    // We only care about N/missing nucleotides
    int codeForN = panmanUtils::getCodeFromNucleotide('N');

    for (const auto& curMut: node->nucMutation) {
        // Based on printSingleNodeHelper() in fasta.cpp
        uint32_t type = (curMut.mutInfo & 0x7);
        // <3 NX types may be multibase, >= 3 NSNPX types are all one base
        int len = type < 3 ? (curMut.mutInfo >> 4) : 1;

        if (type == panmanUtils::NucMutationType::NSNPS) {
            // Nucleotide codes are bitshifted by 20 for some reason
            if (((curMut.nucs >> 20) & 0xF) == codeForN) {
                snvs[node].insert(SNVPosition(curMut));
            }
        } else if (type == panmanUtils::NucMutationType::NS) {
            for(int i = 0; i < len; i++) {
                // Peel away layers to extract a single nucleotide
                if (((curMut.nucs >> (4*(5-i))) & 0xF) == codeForN) {
                    // Make sure to offset the position coordinate
                    snvs[node].insert(SNVPosition(curMut, i));
                }
            }
        } else if (type == panmanUtils::NucMutationType::NSNPI
                   || type == panmanUtils::NucMutationType::NI) {
            IndelPosition curMutPos = IndelPosition(curMut, codeForN);
            // Add new NucMutPosition if we can't merge this one with the back
            if (insertionsSoFar.empty() || !insertionsSoFar.back().mergeIndels(curMutPos)) {
                insertionsSoFar.push_back(curMutPos);
            }
        }
    }

    // Convert vector to set
    insertions[node] = std::unordered_set< panmanUtils::IndelPosition >();
    for (const auto& curIndel : insertionsSoFar) {
        // If at least one value is true (i.e. it's not true that none are true)
        if (!std::none_of(curIndel.isSpecialNuc.begin(), curIndel.isSpecialNuc.end(), [](bool v) { return v; })) {
            insertions[node].insert(curIndel);
        }
    }

    for(auto child: node->children) {
        findMutationsToN(child, snvs, insertions);
    }
}

std::string panmanUtils::Tree::imputeSNV(panmanUtils::Node* node,
        panmanUtils::SNVPosition mutToN, panmanUtils::Node* childToIgnore, int32_t distanceSoFar,
        const std::unordered_map< panmanUtils::Node*, std::unordered_set< panmanUtils::SNVPosition > >& mutsToIgnore) {
    std::cout << "Imputing SNV for " << node->identifier << " pos (" << mutToN.primaryBlockId;
    std::cout << ", " << mutToN.nucPosition << ", " << mutToN.nucGapPosition << ")" << std::endl;
    if (node == nullptr || distanceSoFar > MAX_SNV_IMPUTE_DIST) return "";

    return "";
}

std::string panmanUtils::Tree::imputeInsertion(panmanUtils::Node* node,
        panmanUtils::IndelPosition mutToN, panmanUtils::Node* childToIgnore, int32_t distanceSoFar,
        const std::unordered_map< panmanUtils::Node*, std::unordered_set< panmanUtils::IndelPosition > >& mutsToIgnore) {
    std::cout << "Imputing indel (length " << mutToN.indelLength << ") for " << node->identifier << " pos (" << mutToN.primaryBlockId;
    std::cout << ", " << mutToN.nucPosition << ", " << mutToN.nucGapPosition << ")" << std::endl;
    if (node == nullptr || distanceSoFar > MAX_INSERTION_IMPUTE_DIST) return "";

    return "";
}