#include <iostream>
#include <list>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <fstream>
#include <map>
#include <sstream>
// Global Variables
int BLOCKSIZE;  // Size of the block in the system

bool DEBUG = true;     // If on, prints debug statements

class Ldisk{
    private:
        // Node that saves disk nodes
        class LdiskNode{
            public:
                bool free;          // Determines if the node is free or not.
                std::vector<int> block_set;

                // Generates vector based on first element and size
                std::vector<int> genVector(int first, int size){
                    std::vector<int> result;
                    block_set.resize(size);
                    for(int i = 0; i < size; i++){
                        result[i] = first + i;
                    }
                    return result;
                }

                LdiskNode(bool free, int first, int size) : free(free){
                    block_set = genVector(first, size);
                }

                // Prints out LdiskNode in the format specified by PDF
                void print(){
                    if(free) std::cout << "Free: ";
                    else std::cout << "In use: ";
                    std::cout << block_set.front() << "-" << block_set.back() << std::endl;
                }

                // Helpful in combining two nodes
                void append(std::vector<int> append_set){
                    block_set.insert(block_set.end(), append_set.begin(), append_set.end());
                }

                // Cuts out size elements and returns it as a class
                LdiskNode split(int size){
                    int first_block = block_set[0];
                    block_set = genVector(block_set[size], block_set.size()-size);
                    return LdiskNode(this->free, first_block, size);
                }
        };

        std::list<LdiskNode> nodes;

    public:
        // Prints out disk footprint
        void diskFootprint(){
            for(std::list<LdiskNode>::iterator it=nodes.begin(); it != nodes.end(); ++it){
                it->print();
            }
            // Todo: get disk fragmentation
        }

        // Initiates Ldisk. We do this after BLOCKSIZE is set.
        void initLdisk(int block_count){
            nodes.push_front(LdiskNode(true, 0, block_count));
        }

        // Finds free node and returns its block_set, then sets the free node to occupied
        std::list<LdiskNode>::iterator findFree(){
            std::list<LdiskNode>::iterator it = nodes.begin();
            while(!(it->free)){
                ++it;
            }
            return it;
        }
        // Splits the node at the iterator by the size of the first half, then returns iterator to the first half.
        std::list<LdiskNode>::iterator split(std::list<LdiskNode>::iterator it, int size){
            LdiskNode first_half = it->split(size);     // Gets the the first half
            return nodes.insert(it, first_half);
        }

        // Recombine contiguous free/occupied nodes
        void recombine(){
            std::list<LdiskNode> new_nodes; // Represents new nodes list
            int mode=-1;                    // 1 is Free mode, 0 is Occupied mode, -1 is Unset

            for(std::list<LdiskNode>::iterator it=nodes.begin(); it != nodes.end(); ++it){
                // What to do if the previous one was free
                bool free = it->free;
                if(mode == 1){
                    if(free){
                        new_nodes.back().append(it->block_set);
                    }else{
                        mode = 0;
                        new_nodes.push_back(*it);
                    }
                // What to do if the previous one was occupied
                }else if(mode == 0){
                    if( !(free) ){
                        new_nodes.back().append(it->block_set);
                    }else{
                        mode = 1;
                        new_nodes.push_back(*it);
                    }
                // What to do if this is the first iteration
                }else{
                    if(free) mode = 1;
                    else mode = 0;
                    new_nodes.push_front(*it);
                }
            }
            nodes = new_nodes;
        }

        // Fill free blocks then returns the vector of all the filled blocks
        std::vector<int> fillFree(int size){
            std::vector<int> blocks;
            std::list<LdiskNode>::iterator it;
            while(size > 0){
                it = findFree();
                if(size >= it->block_set.size()){
                    size -= it->block_set.size();
                }else{
                    size = 0;
                    it = split(it, size);   // Splits then returns the first block of the split
                }
                it->free = false;
                std::copy(it->block_set.begin(), it->block_set.end(), std::back_inserter(blocks));  // Appends vector elements to blocks
            }
            recombine();
            return blocks;
        }

};

Ldisk LDISK;

class Lfile{
    private:
        std::list<int> addresses;
    public:
        void initLfile(int filesize) {
            int block_count = filesize / BLOCKSIZE;
            std::vector<int> blocks = LDISK.fillFree(block_count);
            std::copy(blocks.begin(), blocks.end(), std::back_inserter(addresses));  // Appends vector elements to addresses
        }
};

class FileTree{
    private:
        struct TreeNode{
            std::string name;
            bool is_dir;

            // Only for directories
            std::map<std::string, TreeNode> nodes; 

            // Only for fIles
            int size;
            std::string time;
            Lfile lfile;
        };
        TreeNode root;
    public:
        FileTree(){
            root.name = "./";
            root.is_dir = true;
        }

        // Gets the height of the tree. Used in breadth first traversal.
        int getHeight(){
            getHeightHelper(root, 0);
        }
        int getHeightHelper(TreeNode tree, int height){
            if(tree.nodes.size() > 0){
                int max = height+1;
                for(std::map<std::string, TreeNode>::iterator it=tree.nodes.begin(); it != tree.nodes.end(); ++it){
                    if(it->second.is_dir){
                        int dir_height = getHeightHelper(it->second, height+1);
                        if(dir_height > max){
                            max = dir_height;
                        }
                    }
                }
                return max;
            }else return height;
        }

        // Prints directory structure breadth first
        void print(){
            for(int i = 0; i < getHeight(); i++){
                printHelper(root, i);
                std::cout << std::endl;
            }
        }
        void printHelper(TreeNode tree, int depth){
            if(depth != 0){
                for(std::map<std::string, TreeNode>::iterator it=tree.nodes.begin(); it != tree.nodes.end(); ++it){
                    if(it->second.is_dir){
                        printHelper(it->second, depth-1);
                    }
                }
            }else std::cout << tree.name << " ";
        }

        // Finds the directory to add new files/folders into
        TreeNode* getDirectory(std::string path){
            bool found = false;
            int loc;
            TreeNode *cur_node = &root;
            std::string name;
            path.erase(0,2);    //Since we're already at root
            do{
                // If it hasn't reached the end yet
                if((loc = path.find("/")) != std::string::npos){
                    name = path.substr(0, loc);
                    path.erase(0, loc+1);           // +1 to delete the slash

                    // Add a directory if missing or get the directory
                    if(cur_node->nodes.find(name) != cur_node->nodes.end()) cur_node = &cur_node->nodes[name];
                    else{
                        TreeNode new_dir;
                        new_dir.name = name;
                        new_dir.is_dir = true;
                        cur_node->nodes[name] = new_dir;
                        cur_node = &cur_node->nodes[name];
                    }
                }
            }while(!found);

            return cur_node;
        }

        // Adds a directory
        void addDirectory(std::string full_path){
            int loc = full_path.rfind("/");
            std::string name = full_path.substr(loc+1);
            std::string path = full_path.substr(0, loc+1);
            TreeNode* node = getDirectory(path);           // Creates directory if missing.
            TreeNode new_dir;
            new_dir.name = name;
            new_dir.is_dir = true;
            node->nodes[name] = new_dir;
        }

        // Adds a file
        void addFile(std::string full_path, int size, std::string time){
            int loc = full_path.rfind("/");
            std::string name = full_path.substr(loc+1);
            std::string path = full_path.substr(0, loc+1);
            TreeNode* node = getDirectory(path);           // Creates directory if missing. 
            TreeNode new_file;
            new_file.name = name;
            new_file.is_dir = false;
            new_file.size = size;
            new_file.time = time;
            new_file.lfile.initLfile(size);
            node->nodes[name] = new_file;
        }
};

int main(int argc, char* argv[]){
    if (argc != 9) {
        std::cout   << "Usage ./assign2 "
                    << "-f [input file storing information on files] "
                    << "-d [input file storing information on directories] "
                    << "-s [disk size] "
                    << "-b [block size]"
                    << std::endl;
        return -1;
    }

    // Find the indexies of our flags
    int findex = 0; // Index of flag -f
    int dindex = 0; // Index of flag -d
    int sindex = 0; // Index of flag -s
    int bindex = 0; // Index of flag -b
    for(int i = 1; i < argc; i++){
        // I check if index is 0 first for efficiency
        if(strcmp(argv[i], "-f") == 0){
            findex = i;
        }else if(strcmp(argv[i], "-d") == 0){
            dindex = i;
        }else if(strcmp(argv[i], "-s") == 0){
            sindex = i;
        }else if(strcmp(argv[i], "-b") == 0){
            bindex = i;
        }
    }

    // Save all of our argument variables
    std::string file_list = argv[findex+1];
    std::string dir_list = argv[dindex+1];
    int disk_size = atoi(argv[sindex+1]);
    BLOCKSIZE = atoi(argv[bindex+1]);
    int block_count = disk_size/BLOCKSIZE;

    // Directory Tree
    FileTree tree;

    std::ifstream files(file_list);
    std::string item;
    std::string path;
    std::string time;
    int size;
    if(files.is_open()){
        int column;
        while(getline(files, item)){
            std::stringstream ss(item);
            column = -1;
            path = "";
            time = "";
            while(getline(ss, item, ' ')){
                if(item.length() != 0){
                    ++column;
                    if (DEBUG) std::cout << "Column: " << column << std::endl;
                    if (DEBUG) std::cout << item << std::endl;
                    if (DEBUG) std::cout << item.length() << std::endl;
                    if (DEBUG){
                        char ch[2];
                        fgets(ch, 2, stdin);
                    }
                    if (column == 6){
                        std::stringstream integer(item);
                        integer >> size;
                        // if (DEBUG) std::cout << "Column 6: " << size << std::endl;
                    }else if(column == 7 && column == 8 && column == 9){
                        time += item + " ";
                    }else if(column >= 10){
                        path += item + " ";
                    }
                }
            }
            tree.addFile(path, size, time);
        }
    }
    files.close();
    return 0;
}
