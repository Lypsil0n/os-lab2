#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <cstdint>
#include "fs.h"

#define FAT_EOF -1

FS::FS()
{
    read_dir_from_disk(0);

    uint8_t block2[BLOCK_SIZE] = {0};

    disk.read(1, block2);

    std::memcpy(fat, block2, sizeof(fat));
}

FS::~FS()
{
    while(current_working_block != 0){
        cd("..");
    }

    write_dir_to_disk(0);
}

int FS::find_empty_block()
{
    for (int i = 0; i < BLOCK_SIZE / 2; i++)
    {
        if (fat[i] == 0x0000)
        {
            return i;
        }
    }
    return -1;
}

void FS::write_fat_to_disk()
{
    uint8_t block[BLOCK_SIZE] = {0};

    std::memcpy(block, fat, sizeof(fat));

    disk.write(1, block);
}

void FS::write_dir_to_disk(int block_nr)
{
    uint8_t block[BLOCK_SIZE] = {0};

    std::memcpy(block, dir_entries, sizeof(dir_entries));

    disk.write(block_nr, block);
}

void FS::read_dir_from_disk(int block_nr)
{
    uint8_t block[BLOCK_SIZE] = {0};

    disk.read(block_nr, block);

    std::memcpy(dir_entries, block, sizeof(dir_entries));
}

int FS::move_to_path(std::string path_to_move){
    std::vector<std::string> dirs; 
    std::stringstream ss(path_to_move);
    std::string part;
    int block_to_return;

    write_dir_to_disk(current_working_block);

    while(std::getline(ss, part, '/')){
        if(!part.empty()){
            dirs.push_back(part);
        }
    }

    if(path_to_move.at(0) == '/'){
        uint8_t block[BLOCK_SIZE] = {0};

        disk.read(0, block);

        std::memcpy(dir_entries, block, sizeof(dir_entries));
    }

    for(std::string dir : dirs){
        int found = 0;
        for (const struct dir_entry var : dir_entries) {
            if (std::string(var.file_name) == dir && var.type == 1) {
                found = 1;
                block_to_return = var.first_blk;
                uint8_t block[BLOCK_SIZE] = {0};
                disk.read(var.first_blk, block);
                std::memcpy(dir_entries, block, sizeof(dir_entries));
                break;
            }
        }
        if(found == 0){
            return -1;
        }
    }
    return block_to_return;
}

int
FS::check_name_exists(std::string filename){
    int block_to_enter = 1;
    for(struct dir_entry var : dir_entries){
        if(std::string(var.file_name) == filename){
            if(var.type == 1){
                block_to_enter = var.first_blk;
            }
            else {
                return -1;
            }
        }
    } 
    return block_to_enter;
}

int
FS::create_file(std::string data, std::string filepath, std::string og_name = ""){
    bool isSpace = false;
    std::string filename = filepath;
    int block_to_return;

    std::size_t pos = filepath.find_last_of("/");
    if(pos != std::string::npos){
        filename = filepath.substr(pos + 1);
        filepath.erase(pos);
        block_to_return = move_to_path(filepath);
        int check_file = check_name_exists(filename);
        if(og_name != "" && check_file != 1){
            block_to_return = check_file;
            read_dir_from_disk(block_to_return);
            filename = og_name;
        }
    }

    if (filename.length() >= 56)
    {
        return -1;
    }

    for (struct dir_entry var : dir_entries)
    {
        if (!var.file_name[0])
        {
            isSpace = true;
        }
        if (std::string(var.file_name) == filename)
        {
            return -1;
        }
    }

    if (!isSpace)
    {
        return -1;
    }

    size_t data_size = data.size();
    int current_index = 0;
    int num_blocks = std::ceil((double)data_size / BLOCK_SIZE);
    int empty_index = find_empty_block();
    int last_empty_index = -1;
    int first_block = empty_index;
    
    for(int i = 0; i < num_blocks; i++){
        std::vector<char> block(BLOCK_SIZE, 0);
    
        if (empty_index == -1) {
            return -1;
        }

        last_empty_index = empty_index;
        if(i == num_blocks - 1){
            fat[empty_index] = FAT_EOF;
        } else {
            fat[last_empty_index] = empty_index + 1;
        }
        
        size_t remaining_data_size = data_size - current_index;
        size_t copy_size = std::min(remaining_data_size, (size_t)BLOCK_SIZE);

        std::memcpy(block.data(), data.c_str() + current_index, copy_size);
        disk.write(empty_index, reinterpret_cast<uint8_t*>(block.data()));
        write_fat_to_disk();
        current_index += copy_size;
        empty_index = find_empty_block();
    }

    for(struct dir_entry &var : dir_entries){
        if(!var.file_name[0]){
            std::strncpy(var.file_name, filename.c_str(), sizeof(var.file_name) - 1);
            var.file_name[sizeof(var.file_name) - 1] = '\0';
            var.size = data_size;
            var.first_blk = first_block;
            var.type = 0;
            var.access_rights = 0x04;
            break;
        }
    }  

    if(pos != std::string::npos){
        write_dir_to_disk(block_to_return);
        read_dir_from_disk(current_working_block);
    }

    return 0;
}

std::string
FS::read_file(std::string filepath) {
    int i = -1;
    std::string data;
    std::vector<std::vector<uint8_t>> write_data;

    for (const struct dir_entry& var : dir_entries) {
        if (std::string(var.file_name) == filepath) {
            i = var.first_blk;
            break;
        }
    }

    if (i != -1) {
        do {
            std::vector<uint8_t> block(BLOCK_SIZE, 0);

            disk.read(i, block.data());

            block[BLOCK_SIZE - 1] = '\0';

            write_data.push_back(block);

            i = fat[i];
        } while (i != FAT_EOF);
    }

    int actual_file_size = 0;
    for (const struct dir_entry& var : dir_entries) {
        if (std::string(var.file_name) == filepath) {
            actual_file_size = var.size;
            break;
        }
    }

    size_t bytes_to_read = actual_file_size;
    for (const auto& block : write_data) {
        size_t bytes_in_block = std::min(bytes_to_read, (size_t)BLOCK_SIZE);
        data.append(block.begin(), block.begin() + bytes_in_block);
        bytes_to_read -= bytes_in_block;

        if (bytes_to_read == 0) {
            break;
        }
    }

    return data;
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    for (int16_t &var : fat)
    {
        var = 0x0000;
    }
    fat[0] = 0xFFFF;
    fat[1] = 0xFFFF;

    disk.write(1, reinterpret_cast<uint8_t *>(fat));

    for (struct dir_entry &var : dir_entries)
    {
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
    check_name_exists(filepath);

    std::string input;
    std::string data;
    while (std::getline(std::cin, input))
    {
        if (input.empty())
        {
            break;
        }
        data.append(input + "\n");
    }

    return create_file(data, filepath);
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath) {   
    std::string read_data = read_file(filepath);
    
    if (read_data.empty()) {
        return -1;
    }

    std::cout << read_data << std::endl;

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int 
FS::ls()
{
    std::cout << std::left << std::setw(9) << "name" << std::setw(8) << "type" << std::setw(8) << "size" << std::endl;
    for (struct dir_entry var : dir_entries)
    {
        if (var.file_name[0])
        {
            if(var.type == 0){
                std::cout << std::left << std::setw(7) << var.file_name << "   " << std::setw(6) << "file" << "   " << std::setw(6) << var.size << std::endl;
            } else {
                std::cout << std::left << std::setw(7) << var.file_name << "   " << std::setw(6) << "dir" << "   " << std::setw(6) << "-" << std::endl;
            }
            
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
    std::string read_data = read_file(sourcepath);
    int block_to_enter = check_name_exists(destpath);

    if (read_data.empty()) {
        return -1;
    }

    if(block_to_enter == -1){
        return -1;
    } else if(block_to_enter != 1){
        destpath.append("/" + sourcepath);
    }

    create_file(read_data, destpath, sourcepath);

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    struct dir_entry old_entry;
    int i = 0;

    for (struct dir_entry &var : dir_entries) {
        if (std::string(var.file_name) == sourcepath) {
            old_entry = var;
            std::memset(var.file_name, 0, sizeof(var.file_name));
            var.size = 0;
            var.first_blk = 0;
            var.type = 0;
            var.access_rights = 0;
            break;
        }
        i++;
    }
    write_dir_to_disk(current_working_block);

    std::string filename = destpath;
    std::size_t pos = destpath.find_last_of("/");
    int block_to_return;
    if(pos != std::string::npos){
        filename = destpath.substr(pos + 1);
        destpath.erase(pos);
        block_to_return = move_to_path(destpath);
    }
    

    int block_to_enter = check_name_exists(filename);

    if(block_to_enter == -1) {
        return -1;
    } else if(block_to_enter != 1) {
        read_dir_from_disk(block_to_enter);
        if(check_name_exists(sourcepath) == 1){
            for(struct dir_entry &var : dir_entries){
                if(!var.file_name[0]){
                    var = old_entry;
                    break;
                }
            }
            write_dir_to_disk(block_to_enter);
        } else {
            read_dir_from_disk(current_working_block);
            return -1;
        }
        read_dir_from_disk(current_working_block);
    } else if(block_to_enter == 1) {
        for(struct dir_entry &var : dir_entries){
            if(!var.file_name[0]){
                var = old_entry;
                std::strncpy(var.file_name, filename.c_str(), sizeof(var.file_name) - 1);
                var.file_name[sizeof(var.file_name) - 1] = '\0';
                break;
            }
        }
        write_dir_to_disk(block_to_return);
        read_dir_from_disk(current_working_block);
    } else {
        dir_entries[i] = old_entry;
    }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    // if(check_name_exists(filepath) == -1){
    //     return -1;
    // }

    int current_block = 0;
    int next_block = 0;
    
    for(struct dir_entry &var : dir_entries){
        if(std::string(var.file_name) == filepath){
            current_block = var.first_blk;
            std::memset(var.file_name, 0, sizeof(var.file_name));
            var.size = 0;
            var.first_blk = 0;
            var.type = 0;
            var.access_rights = 0;
            break;
        }
    }

    if(fat[current_block] == FAT_EOF){
        fat[current_block] = 0x0000;
    } else {
        do{
        next_block = fat[current_block];
        fat[current_block] = 0x0000;
        current_block = next_block;
        } while(next_block != FAT_EOF);

        fat[next_block] = 0x0000;
    }

    write_fat_to_disk();

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::string file1 = read_file(filepath1);
    std::string file2 = read_file(filepath2);

    rm(filepath2);

    create_file(file2 + file1, filepath2);

    write_fat_to_disk();

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    if(check_name_exists(dirpath) == -1) {
        return -1;
    };

    int first_block = find_empty_block();

    for(struct dir_entry &var : dir_entries){
        if(!var.file_name[0]){
            std::strncpy(var.file_name, dirpath.c_str(), sizeof(var.file_name) - 1);
            var.file_name[sizeof(var.file_name) - 1] = '\0';
            var.size = 0;
            var.first_blk = first_block;
            var.type = 1;
            var.access_rights = 0x04;
            break;
        }
    }  

    uint8_t block[BLOCK_SIZE] = {0};

    struct dir_entry sub_dir_entries[BLOCK_SIZE / sizeof(struct dir_entry)];

    for (struct dir_entry &var : sub_dir_entries)
    {
        std::memset(var.file_name, 0, sizeof(var.file_name));
        var.size = 0;
        var.first_blk = 0;
        var.type = 0;
        var.access_rights = 0;
    }

    std::strncpy(sub_dir_entries[0].file_name, "..", sizeof(sub_dir_entries[0].file_name) - 1);
    sub_dir_entries[0].file_name[sizeof(sub_dir_entries[0].file_name) - 1] = '\0';
    sub_dir_entries[0].size = 0;
    sub_dir_entries[0].first_blk = current_working_block;
    sub_dir_entries[0].type = 1;
    sub_dir_entries[0].access_rights = 0x04;

    std::memcpy(block, sub_dir_entries, sizeof(sub_dir_entries));

    disk.write(first_block, block);

    fat[first_block] = FAT_EOF;
    write_fat_to_disk();

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    int current_block = -1;

    for (const struct dir_entry var : dir_entries) {
        if (std::string(var.file_name) == dirpath && var.type == 1) {
            if(var.type == 1){
                current_block = var.first_blk;
                if(std::string(var.file_name) != ".."){
                    if(current_working_block != 0){
                        path.append("/");
                    }
                    path.append(std::string(var.file_name));
                } else {
                    size_t pos = path.find_last_of("/");
                    path.erase(pos);
                    if(current_block == 0){
                        path.append("/");
                    }
                }
            } else {
                return -1;
            }
            break;
        }
    }

    if(current_block != -1){
        write_dir_to_disk(current_working_block);

        uint8_t block[BLOCK_SIZE] = {0};

        disk.read(current_block, block);

        std::memcpy(dir_entries, block, sizeof(dir_entries));

        current_working_block = current_block;
    } else {
        return -1;
    }

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << path << std::endl;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
