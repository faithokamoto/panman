#include <iostream>
#include <chrono>
#include <filesystem>

#include <fstream>

#include "PangenomeMAT.hpp"

std::vector< std::string > splitString(const std::string& s){
    std::vector< std::string > res;
    std::string current;

    for(size_t i = 0; i < s.length(); i++){
        if(s[i] != ' '){
            current += s[i];
        } else {
            res.push_back(current);
            current = "";
        }
    }

    if(current.length()){
        res.push_back(current);
        current = "";
    }

    return res;
}

int main(int argc, char* argv[]){
    if(argc < 2){
        std::cout << "Please provide file name.\n";
        return -1;
    }

    try {
        std::ifstream input(argv[1]);

        auto treeBuiltStart = std::chrono::high_resolution_clock::now();
        
        PangenomeMAT::Tree T(input);

        auto treeBuiltEnd= std::chrono::high_resolution_clock::now();
        std::chrono::nanoseconds treeBuiltTime = treeBuiltEnd - treeBuiltStart;

        std::cout << "Data load time: " << treeBuiltTime.count() << '\n';
        
        while(true){
            std::cout << "> ";

            std::string command;
            std::getline (std::cin, command);

            std::vector< std::string > splitCommand = splitString(command);

            if(splitCommand.size() == 1 && splitCommand[0] == "summary"){
                auto summaryStart = std::chrono::high_resolution_clock::now();
                T.printSummary();
                auto summaryEnd = std::chrono::high_resolution_clock::now();

                std::chrono::nanoseconds summaryTime = summaryEnd - summaryStart;

                std::cout << "\nSummary creation time: " << summaryTime.count() << '\n';       
            } else if(splitCommand.size() == 2 && splitCommand[0] == "fasta"){
                std::string fileName = splitCommand[1];
                std::filesystem::create_directory("./fasta");
                std::ofstream fout("./fasta/" + fileName + ".fasta");

                T.printFASTA(fout);

                fout.close();
            } else if(splitCommand.size() == 1 && splitCommand[0] == "exit"){
                return 0;
            }
        }

        // T.printBfs();

    } catch(std::exception& e){
        std::cout << e.what() << std::endl;
        return -1;
    }

}