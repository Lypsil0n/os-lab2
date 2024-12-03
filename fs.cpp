#include <iostream>
#include <cstring>
#include <string>
#include <iomanip>
#include <cmath>
#include <vector>
#include <cstdint>
#include "fs.h"

#define FAT_EOF -1

FS::FS()
{
    uint8_t block[BLOCK_SIZE] = {0};

    disk.read(0, block);

    std::memcpy(dir_entries, block, sizeof(dir_entries));

    uint8_t block2[BLOCK_SIZE] = {0};

    disk.read(1, block2);

    std::memcpy(fat, block2, sizeof(fat));
}

FS::~FS()
{
    uint8_t block[BLOCK_SIZE] = {0};

    std::memcpy(block, dir_entries, sizeof(dir_entries));

    disk.write(0, block);
}

int
FS::find_empty_block()
{
    for(int i = 0; i < BLOCK_SIZE/2; i++){
        if(fat[i] == 0x0000){
            return i;
        }
    }
    return -1;
}

void
FS::write_fat_to_disk(){
    uint8_t block[BLOCK_SIZE] = {0};

    std::memcpy(block, fat, sizeof(fat));

    disk.write(1, block);
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    for(int16_t &var : fat){
        var = 0x0000;
    }
    fat[0] = 0xFFFF;
    fat[1] = 0xFFFF;

    disk.write(1, reinterpret_cast<uint8_t*>(fat));

    for(struct dir_entry &var : dir_entries){
        std::memset(var.file_name, 0, sizeof(var.file_name));
        var.size = 0;
        var.first_blk = 0;
        var.type = 0;
        var.access_rights = 0;
    }
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{   
    for(struct dir_entry var : dir_entries){
        if(std::string(var.file_name) == filepath){
            std::cout << "Filename already exist!";
            return -1;
        }
    }  

    std::string input;
    std::string data;
    while(std::getline(std::cin, input)){
        if(input.empty()){
            break;
        }
        data.append(input + "\n");
    }

    size_t data_size = data.size();
    int current_index = 0;
    int num_blocks = std::ceil((double)data_size / BLOCK_SIZE);
    int empty_index = -1;
    int last_empty_index = -1;
    int first_block;
    
    for(int i = 0; i < num_blocks; i++){
        std::vector<char> block(BLOCK_SIZE, 0);
    
        empty_index = find_empty_block();
        if (empty_index == -1) {
            std::cout << "No empty blocks available!" << std::endl;
            return -1;
        }

        if(i == num_blocks - 1){
            fat[empty_index] = FAT_EOF;
        } else if(last_empty_index != -1){
            fat[last_empty_index] = empty_index;
            last_empty_index = empty_index;
        }

        first_block = empty_index;
        
        size_t remaining_data_size = data_size - current_index;
        size_t copy_size = std::min(remaining_data_size, (size_t)BLOCK_SIZE);

        std::memcpy(block.data(), data.c_str() + current_index, copy_size);

        disk.write(empty_index, reinterpret_cast<uint8_t*>(block.data()));
        write_fat_to_disk();
        current_index += copy_size;
    }

    for(struct dir_entry &var : dir_entries){
        if(!var.file_name[0]){
            std::strncpy(var.file_name, filepath.c_str(), sizeof(var.file_name) - 1);
            var.file_name[sizeof(var.file_name) - 1] = '\0';
            var.size = data_size;
            var.first_blk = first_block;
            var.type = 0;
            var.access_rights = 0x04;
            break;
        }
    }   

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    int i = -1;
    for(struct dir_entry var : dir_entries){
        if(std::string(var.file_name) == filepath){
            i = var.first_blk;
            break;
        }
    }
    if(i == -1){
        return -1;
    }
    do {
        uint8_t block[BLOCK_SIZE] = {0};

        disk.read(i, block);

        block[BLOCK_SIZE - 1] = '\0';

        std::cout << block << std::endl;

        i = fat[i];
    } while(i != FAT_EOF);
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{   
    std::cout << std::left << std::setw(9) << "name" << std::setw(9) << "size" << std::endl;
    for(struct dir_entry var : dir_entries){
        if(var.file_name[0]){
            std::cout << std::left << std::setw(9) << std::string(var.file_name) << std::setw(9) << var.size << std::endl;
        }
    }   
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "/";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}