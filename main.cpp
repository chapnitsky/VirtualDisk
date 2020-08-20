#include <iostream>
#include <vector>
#include <map>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define DISK_SIZE 256


void  decToBinary(int n , char &c){ 
   // array to store binary number 
    int binaryNum[8]; 
  
    // counter for binary array 
    int i = 0; 
    while (n > 0) { 
          // storing remainder in binary array 
        binaryNum[i] = n % 2; 
        n = n / 2; 
        i++; 
    } 
  
    // printing binary array in reverse order 
    for (int j = i - 1; j >= 0; j--) {
        if (binaryNum[j]==1)
            c = c | 1u << j;
    }
} 

// #define SYS_CALL
// ============================================================================
class Inode {
    int fileSize;
    int total_blocks;

    int *directBlocks;
    int singleInDirect;
    int num_of_direct_blocks;
    int block_size;


    public:
    Inode(int _block_size, int _num_of_direct_blocks) {
        fileSize = 0; 
        total_blocks = 0; 
        block_size = _block_size;
        num_of_direct_blocks = _num_of_direct_blocks;
        directBlocks = new int[num_of_direct_blocks];
        assert(directBlocks);
        for (int i = 0 ; i < num_of_direct_blocks; i++) {   
            directBlocks[i] = -1;
        }
        singleInDirect = -1;
    }

    int getFileSize(){
        return fileSize;
    }
    int getTotalBlocks(){
        return total_blocks;
    }
    int* getDirect(){
        return directBlocks;
    }
    int getInDirect(){
        return singleInDirect;
    }
    int getNumOfDirect(){
        return num_of_direct_blocks;
    }
    void setFileSize(int size){
        fileSize = size;
    }
    void setBlockInUse(int num){
        total_blocks = num;
    }
    void setInDirect(int num){
        singleInDirect = num;
    }
    void setNumOfDirect(int num){
        num_of_direct_blocks = num;
    }
    ~Inode() { 
        delete directBlocks;
    }


};

// ============================================================================
class FileDescriptor {
    pair<string, Inode*> file;
    bool inUse;//open = true, closed = false

    public:
    FileDescriptor(string FileName, Inode* fsi) {
        file.first = FileName;
        file.second = fsi;
        inUse = true;

    }

    string getFileName() {
        return file.first;
    }

    void deleteName(){
        file.first = "";
    }
    Inode* getInode() {
        
        return file.second;

    }


    bool isInUse() { 
        return (inUse); 
    }
    void setInUse(bool _inUse) {
        inUse = _inUse ;
    }

};
 
#define DISK_SIM_FILE "DISK_SIM_FILE.txt"
// ============================================================================
class Disk {
    FILE *sim_disk_fd;
    bool is_formated;

	// BitVector - "bit" (int) vector, indicate which block in the disk is free
	//              or not.  (i.e. if BitVector[0] == 1 , means that the 
	//             first block is occupied. 
    int BitVectorSize;
    int *BitVector;

    // Unix directories are lists of association structures, 
    // each of which contains one filename and one inode number.
    map<string, Inode*>  MainDir; 

    // openFD --  when you open a file, 
	// the operating system creates an entry to represent that file
    // This entry number is the file descriptor. 
    vector<FileDescriptor> openFD;

    int direct_enteris;
    int block_size;
    int disk_fd;//For lseek, write and read
    private:

        void setFreeBlock(int i){
            BitVector[i] = 0;
        }
        int getFreeBlock(){
            for(int i = 0; i < BitVectorSize; i++)
                if(BitVector[i] == 0){
                    BitVector[i] = 1;//Taken
                    return i;
                }
                    
            return -1;
        }

        bool EnoughSpace(int required_blocks){
            int count = 0;
            for(int i = 0; i < BitVectorSize; i++)
                if(BitVector[i] == 0)//Free
                    count++;
            
            if(count >= required_blocks)
                return true;
            return false;
        }
        char* checkLastDirectBlock(int i, int* arr, char* p, int &req_blocks){
            char *read_buff = (char*)malloc(block_size*sizeof(char) + 1), *write_buff = (char*)malloc(block_size*sizeof(char) + 1), *p1 = read_buff;
            assert(read_buff != NULL);
            assert(write_buff != NULL);
            memset(read_buff, 0, block_size);
            memset(write_buff, 0, block_size);
            read_buff[block_size] = 0;//NULL
            write_buff[block_size] = 0;//NULL
            int j = 0, z = 0;
            assert(lseek(disk_fd, arr[i - 1]*block_size, SEEK_SET) >= 0);
            assert(read(disk_fd, read_buff, block_size) >= 0);
            memset(read_buff + block_size, 0, 1);
            while(*p1 != 0){
                ++j;
                ++p1;
            }
            z = block_size - j;
            int k = 0;
            if(z != 0){//There is an empty room
                while(k < z && *p != 0){//Copying block size characters
                    write_buff[k] = *p;
                    ++p;
                    ++k;
                } 
                assert(lseek(disk_fd, arr[i - 1]*block_size + j, SEEK_SET) >= 0);
                assert(write(disk_fd, write_buff, k) >= 0);
                char *ptemp = p;//Checking if there is a need to decrease the required blocks
                int count = 0;
                while(*ptemp != 0){
                    ++count;
                    if(count > block_size)
                        break;
                    ++ptemp;
                }
                if(k + count > block_size || *p == 0 || p[1] != 0)
                    req_blocks--;
            }
            free(write_buff);
            free(read_buff);
            return p;
        }

        char* checkLastInDirectBlock(int singleIND, char* p, int &req_blocks){
            char *read_buff = (char*)malloc(block_size*sizeof(char) + 1), *write_buff = (char*)malloc(block_size*sizeof(char) + 1), *p1 = read_buff, *Binary2Dec;
            assert(read_buff != NULL);
            assert(write_buff != NULL);
            memset(read_buff, 0, block_size);
            memset(write_buff, 0, block_size);
            read_buff[block_size] = 0;//NULL
            write_buff[block_size] = 0;//NULL
            int j = 0;
            assert(lseek(disk_fd, singleIND, SEEK_SET) >= 0);
            assert(read(disk_fd, Binary2Dec, 1) >= 0);
            int block_ind = (int)*Binary2Dec;
            assert(lseek(disk_fd, block_ind*block_size, SEEK_SET) >= 0);
            assert(read(disk_fd, read_buff, block_size) >= 0);
            memset(read_buff + block_size, 0, 1);
            while(*p1 != 0){
                ++j;
                ++p1;
            }
            int z = block_size - j;
            int k = 0;
            if(z != 0){//There is an empty room
                while(k < z && *p != 0){//Copying block size characters
                    write_buff[k] = *p;
                    ++p;
                    ++k;
                } 
                assert(lseek(disk_fd, block_ind*block_size + j, SEEK_SET) >= 0);
                assert(write(disk_fd, write_buff, k) >= 0);
                char *ptemp = p;//Checking if there is a need to decrease the required blocks
                int count = 0;
                while(*ptemp != 0){
                    ++count;
                    if(count > block_size)
                        break;
                    ++ptemp;
                }
                if(k + count > block_size || *p == 0 || p[1] != 0)
                    req_blocks--;
            }
            free(write_buff);
            free(read_buff);
            return p;
        }

    public:
    // ------------------------------------------------------------------------
    Disk() {
        sim_disk_fd = fopen(DISK_SIM_FILE, "r+" );
        assert(sim_disk_fd);
        for (int i=0; i < DISK_SIZE ; i++) {
            int ret_val = fseek (sim_disk_fd , i , SEEK_SET);
            ret_val = fwrite("\0" ,  1 , 1, sim_disk_fd);
            assert(ret_val == 1);
        }
        fflush(sim_disk_fd);
        disk_fd = fileno(sim_disk_fd);
    }

    ~Disk(){
        free(BitVector);
        fclose(sim_disk_fd);
        for(int i = 0; i < openFD.size(); i++){
            if(openFD.at(i).getFileName() != ""){
                string name = openFD.at(i).getFileName();
                MainDir.erase(name);
            }
            delete openFD.at(i).getInode();
        }

    }
	


    // ------------------------------------------------------------------------
    void listAll() {
        int i = 0;    
        for (auto it = begin(openFD); it != end (openFD); ++it) {
            cout << "index: " << i << ": FileName: " << it->getFileName() <<  " , isInUse: " << it->isInUse() << endl; 
            i++;
        }
        char bufy;
        cout << "Disk content: '" ;
        char temp[DISK_SIZE];
        assert(lseek(disk_fd, 0, SEEK_SET) >= 0);
        assert(read(disk_fd, temp, DISK_SIZE) >= 0);
        for (i=0; i < DISK_SIZE ; i++) 
            if((int)temp[i] > 32)//Not junk data or space
                cout << temp[i];              
        cout << "'" << endl;
    }
 
    // ------------------------------------------------------------------------
    void fsFormat(int blockSize =4, int direct_Enteris_= 3) {
        if(is_formated)
            free(BitVector);
        this->block_size = blockSize;
        this->direct_enteris = direct_Enteris_;
        this->BitVectorSize = DISK_SIZE/block_size;
        BitVector = (int*)malloc(BitVectorSize*sizeof(int));
        assert(BitVector != NULL);
        memset(BitVector, 0, BitVectorSize);
        is_formated = true;
    }

    // ------------------------------------------------------------------------
    int CreateFile(string fileName) {
        if(!is_formated)
            return -1;
        if(MainDir.find(fileName) != MainDir.end()){
            return -1;
        }

        Inode *node = new Inode(this->block_size, this->direct_enteris);
        MainDir.insert({fileName, node});
        openFD.push_back(FileDescriptor(fileName, node));
        return (openFD.size() - 1);
    }

    // ------------------------------------------------------------------------
    int OpenFile(string fileName) {
        if(!is_formated || MainDir.find(fileName) == MainDir.end())
            return -1;
        
        for(int i = 0; i < openFD.size(); i++)
            if(openFD.at(i).getFileName() == fileName){
                bool inUse = openFD.at(i).isInUse();
                if(inUse){
                    return -1;//Already opens
                }else if(!inUse){
                    openFD.at(i).setInUse(true);
                    return i;
                }
            }
    }  

    // ------------------------------------------------------------------------
    string CloseFile(int fd) {
        if(!is_formated || !openFD.at(fd).isInUse() || fd > openFD.size() - 1)
            return "-1";

        string name = openFD.at(fd).getFileName();
        if(MainDir.find(name) == MainDir.end())
            return "-1";
        
        openFD.at(fd).setInUse(false);
        return name;
    }
    // ------------------------------------------------------------------------
    int WriteToFile(int fd, char *buf, int len ) {
        if(!is_formated || !openFD.at(fd).isInUse() || fd > openFD.size() - 1)
            return -1;

        Inode *temp = openFD.at(fd).getInode();
        int cur_size = temp->getFileSize(),
        req_blocks = (int)ceil(len/(double)block_size),
        check = req_blocks,
        direct = temp->getNumOfDirect(),
        indirect = temp->getInDirect(),
        *arr = temp->getDirect();
        int total_blocks = temp->getTotalBlocks();
        if(temp->getInDirect() == -1 && (cur_size + len > direct*block_size))
            check++;//For indirect block index also
        if((cur_size + len > block_size*(block_size + direct_enteris)) || !(EnoughSpace(check)))//Too big or not enough space
            return -1;

        char *p = buf;//Pointer to the str_to_write
        bool flag = true;//To check ONCE if there is an empty room in the last block
        if(req_blocks + total_blocks <= direct){//Case: DIRECT ONLY
            for(int i = total_blocks; i < req_blocks + total_blocks; i++){
                if(flag && i > 0){
                    flag = false;
                    p = checkLastDirectBlock(i, arr, p, req_blocks);
                    if(req_blocks == 0)
                        break;
                }
                char *block_buff = (char*)malloc(block_size*sizeof(char) + 1);
                assert(block_buff != NULL);
                memset(block_buff, 0, block_size);
                block_buff[block_size] = 0;//NULL
                int k = 0;
                while(k < block_size && *p != 0){//Copying block size characters
                    block_buff[k] = *p;
                    ++p;
                    ++k;
                }
                arr[i] = getFreeBlock();
                if(arr[i] == -1)//FULL
                    return -1;
                assert(lseek(disk_fd, arr[i]*block_size, SEEK_SET) >= 0);
                assert(write(disk_fd, block_buff, k) >= 0);
                fflush(sim_disk_fd);
                free(block_buff);
                temp->setBlockInUse(temp->getTotalBlocks() + 1);
            }

        }else{//Case: Both or only INDIRECT
            for(int i = total_blocks; i < direct; i++){//DIRECT
                if(flag && i > 0){
                    flag = false;
                    p = checkLastDirectBlock(i, arr, p, req_blocks);
                    if(req_blocks == 0){
                        temp->setFileSize(cur_size + len);
                        return 0;//Normal exit of function
                    }
                }
                char *block_buff = (char*)malloc(block_size*sizeof(char) + 1);
                assert(block_buff);
                memset(block_buff, 0, block_size);
                block_buff[block_size] = 0;//NULL
                int k = 0;
                while(k < block_size && *p != 0){//Copying block size characters
                    block_buff[k] = *p;
                    ++p;
                    ++k;
                }
                arr[i] = getFreeBlock();
                if(arr[i] == -1)//FULL
                    return -1;
                assert(lseek(disk_fd, arr[i]*block_size, SEEK_SET) >= 0);
                assert(write(disk_fd, block_buff, k) >= 0);
                req_blocks--;
                fflush(sim_disk_fd);
                free(block_buff);
                temp->setBlockInUse(temp->getTotalBlocks() + 1);
            }
            //INDIRECT
            if(indirect == -1){
                temp->setInDirect(getFreeBlock());
                if(temp->getInDirect() == -1)//FULL
                    return -1;
                p = checkLastDirectBlock(direct, arr, p, req_blocks);
                flag = false;
                if(req_blocks == 0){
                    temp->setFileSize(cur_size + len);
                    return 0;//Normal exit of function
                }
                        
            }
            int k = temp->getTotalBlocks() - direct;
            for(int i = k; i < req_blocks + k; i++){
                if(flag && i > 0){
                    flag = false;
                    int ind = temp->getInDirect();
                    p = checkLastInDirectBlock(ind*block_size + k -1, p, req_blocks);
                    if(req_blocks == 0)
                        break;
                }
                char c = '\0', *c_buff = (char*)malloc(sizeof(char)*2);
                assert(c_buff != NULL);
                memset(c_buff, 0, 1);
                c_buff[1] = 0;//NULL
                int num = getFreeBlock();
                if(num == -1)//FULL
                    return -1;
                decToBinary(num, c);
                c_buff[0] = c;
                assert(lseek(disk_fd, temp->getInDirect()*block_size + temp->getTotalBlocks() - this->direct_enteris, SEEK_SET) >= 0);
                assert(write(disk_fd, c_buff, 1) >= 0);//Writing index
                char *block_buff = (char*)malloc(block_size*sizeof(char) + 1);
                assert(block_buff);
                memset(block_buff, 0, block_size);
                block_buff[block_size] = 0;//NULL
                int j = 0;
                while(j < block_size && *p != 0){//Copying block size characters
                    block_buff[j] = *p;
                    ++p;
                    ++j;
                }
                assert(lseek(disk_fd, num*block_size, SEEK_SET) >= 0);
                assert(write(disk_fd, block_buff, j) >= 0);//Writing the buff
                fflush(sim_disk_fd);
                free(c_buff);
                free(block_buff);
                temp->setBlockInUse(temp->getTotalBlocks() + 1);
            }

        }
        temp->setFileSize(cur_size + len);
    }
    // ------------------------------------------------------------------------
    int DelFile( string FileName ) {
        if(MainDir.find(FileName) == MainDir.end())//Not Found
            return -1;

        Inode *temp = MainDir.find(FileName)->second;
        if(temp->getFileSize() == 0)
            return -1;
        int direct = temp->getNumOfDirect(),
         *arr = temp->getDirect(),
          ind = temp->getInDirect(),
          total_blocks = temp->getTotalBlocks();
        temp->setBlockInUse(0);
        temp->setFileSize(0);
        temp->setNumOfDirect(0);
        temp->setInDirect(-1);
        setFreeBlock(ind);//Indirect block index
        for(int i = 0; i < direct; i++)//Resetting DIRECT blocks
            setFreeBlock(arr[i]);
        if(total_blocks > direct){
            char *read_buff = (char*)malloc(block_size*sizeof(char) + 1), *p = read_buff;
            assert(read_buff != NULL);
            memset(read_buff, 0, block_size);
            read_buff[block_size] = 0;//NULL
            assert(lseek(disk_fd, ind*block_size, SEEK_SET) >= 0);
            assert(read(disk_fd, read_buff, block_size) >= 0);

            for(int i = 0; i < block_size; i++){//Resetting INDIRECT blocks
                if(p[i] == 0)
                    break;//End of INDIRECT indexes
                int num = (int)p[i];
                setFreeBlock(num);
            }
            free(read_buff);
        }
        MainDir.find(FileName)->second->~Inode();
        MainDir.erase(FileName);
        int fd = -1;
        for(int i = 0; i < openFD.size(); i++)
            if(openFD.at(i).getFileName() == FileName){
                openFD.at(i).setInUse(false);//Deleted
                openFD.at(i).deleteName();//Deleting the name
                fd = i;
            }

        return fd;
    }
    // ------------------------------------------------------------------------
    int ReadFromFile(int fd, char *buf, int len ) { 
        if(!is_formated || !openFD.at(fd).isInUse() || fd > openFD.size() - 1)
            return -1;
        
        Inode *temp = openFD.at(fd).getInode();
        if(temp->getFileSize() == 0){//Empty file
            buf[0] = 0;
            return 0;
        }
        if(temp->getFileSize() < len)
            len = temp->getFileSize();
        int index = 0;  
        int InLen = len - direct_enteris*block_size, *arr = temp->getDirect(), ind = temp->getInDirect(), total_blocks = temp->getTotalBlocks();//Length to read from INDIRECT blocks
        bool InRead = false;
        char *buff = (char*)malloc(block_size*sizeof(char) + 1);
        assert(buff != NULL);
        buff[block_size] = 0;//NULL

        if(InLen > 0 && ind != -1)
            InRead = true;//Need to read also from INDIRECT blocks
        
        if(!InRead){//DIRECT read only
            for(int i = 0; i < direct_enteris; i++){
                if(len <= 0)
                    break;
                memset(buff, 0, block_size);
                assert(lseek(disk_fd, arr[i]*block_size, SEEK_SET) >= 0);
                assert(read(disk_fd, buff, block_size) >= 0);
                char *ptemp = buff;
                int count = 0;
                while(ptemp != 0 && count < block_size && len > 0){//Copying
                    ++count;
                    buf[index] = *ptemp;
                    ++ptemp;
                    ++index;
                    --len;
                }
                if(i == direct_enteris - 1 || len <= 0)
                    buf[index] = 0;//NULL in the end
            }
        }else{//BOTH
            for(int i = 0; i < direct_enteris; i++){//DIRECT
                if(len <= 0)
                    break;
                memset(buff, 0, block_size);
                assert(lseek(disk_fd, arr[i]*block_size, SEEK_SET) >= 0);
                assert(read(disk_fd, buff, block_size) >= 0);
                char *ptemp = buff;
                int count = 0;
                while(ptemp != 0 && count < block_size && len > 0){//Copying
                    ++count;
                    buf[index] = *ptemp;
                    ++ptemp;
                    ++index;
                    --len;
                }
            }
            char *single_buff = (char*)malloc(block_size*sizeof(char) + 1), *p1 = single_buff;
            assert(single_buff != NULL);
            memset(single_buff, 0, block_size);
            single_buff[block_size] = 0;//NULL
            assert(lseek(disk_fd, ind*block_size, SEEK_SET) >= 0);
            assert(read(disk_fd, single_buff, block_size) >= 0);
            for(int i = 0; i < block_size; i++){//INDIRECT
                if(p1[i] == 0){//Stop copying
                    buf[index] = 0;//NULL in the end
                    break;
                }
                int num = (int)p1[i];//Index
                memset(buff, 0, block_size);
                assert(lseek(disk_fd, num*block_size, SEEK_SET) >= 0);
                assert(read(disk_fd, buff, block_size) >= 0);
                char *ptemp = buff;
                int count = 0;
                while(ptemp != 0 && count < block_size && len > 0){//Copying
                    ++count;
                    buf[index] = *ptemp;
                    ++ptemp;
                    ++index;
                    --len;
                }
                if(i == block_size - 1 || len <= 0)
                    buf[index] = 0;//NULL in the end
            }
            free(single_buff);
        }
        free(buff);
    }
};
    
int main() {
    int blockSize; 
	int direct_entries;
    string fileName;
    char str_to_write[DISK_SIZE];
    char str_to_read[DISK_SIZE];
    int size_to_read; 
    int _fd, l;

    Disk *fs = new Disk();
    int cmd_;

    while(1) {
        cin >> cmd_;
        switch (cmd_)
        {
            case 0:   // exit
				delete fs;
				exit(0);
                break;

            case 1:  // list-file
                fs->listAll(); 
                break;
          
            case 2:    // format
                cin >> blockSize;
				cin >> direct_entries;
                fs->fsFormat(blockSize, direct_entries);
                break;
          
            case 3:    // creat-file
                cin >> fileName;
                _fd = fs->CreateFile(fileName);
                cout << "CreateFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;
            
            case 4:  // open-file
                cin >> fileName;
                _fd = fs->OpenFile(fileName);
                cout << "OpenFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;
             
            case 5:  // close-file
                cin >> _fd;
                fileName = fs->CloseFile(_fd); 
                cout << "CloseFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;
           
            case 6:   // write-file
                cin >> _fd;
                cin >> str_to_write;
                l = strlen(str_to_write);
                fs->WriteToFile(_fd, str_to_write, l);
                break;
          
            case 7:    // read-file
                cin >> _fd;
                cin >> size_to_read ;
                fs->ReadFromFile( _fd , str_to_read , size_to_read );
                cout << "ReadFromFile: " << str_to_read << endl;
                break;
           
            case 8:   // delete file 
                 cin >> fileName;
                _fd = fs->DelFile(fileName);
                cout << "DeletedFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;
            default:
                break;
        }
    }

} 