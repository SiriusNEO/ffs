#include "../include/v_disk.h"

void create_image() {
    FFS_DBG_INFO("No image found. Create new image.\n");

    char *index_pool = (void *) malloc(sizeof(char) * V_DISK_INDEX_SIZE);
    char *bank_pool = (void *) malloc(sizeof(char) * V_DISK_BANK_SIZE);

    super_node_buf = (super_node*) index_pool;

    // set bitmap
    memset(super_node_buf->index_bitmap, 0, MAX_FILE_NUM);
    memset(super_node_buf->bank_bitmap, 0, BLOCK_NUM);

    FFS_DBG_INFO("set bitmap finish. \n");

    super_node_buf->fs_blk_num = 0; 
    super_node_buf->root_inode = sizeof(super_node); // super_node - root_inode - inode1 - inode 2 - ...

    i_node* new_root = (i_node*) (index_pool + super_node_buf->root_inode);

    // init root inode
    super_node_buf->index_bitmap[0] = 1; // root
    new_empty_inode(FS_ROOT, 1, new_root);

    FFS_DBG_INFO("init index_pool finish. \n");

    FILE* index_creator = fopen(V_DISK_INDEX_PATH, "wb");
    FILE* bank_creator = fopen(V_DISK_BANK_PATH, "wb");

    fseek(index_creator, 0, SEEK_SET);
    fseek(bank_creator, 0, SEEK_SET);

    fwrite(index_pool, sizeof(char), V_DISK_INDEX_SIZE, index_creator);
    fwrite(bank_pool, sizeof(char), V_DISK_BANK_SIZE, bank_creator);

    FFS_DBG_INFO("write to image file finish. \n");

    fclose(index_creator);
    fclose(bank_creator);

    free(index_pool);
    free(bank_pool);

    FFS_DBG_INFO("Image created successfully. \n");
    FFS_DBG_INFO("index image = %d(Bytes) in path: %s\n", V_DISK_INDEX_SIZE, V_DISK_INDEX_PATH);
    FFS_DBG_INFO("bank image = %d(Bytes) in path: %s\n", V_DISK_BANK_SIZE, V_DISK_BANK_PATH);
}

void open_image() {
    index_fp = fopen(V_DISK_INDEX_PATH, "rb+");
    bank_fp = fopen(V_DISK_BANK_PATH, "rb+");

    if (index_fp == NULL) {
        create_image();

        index_fp = fopen(V_DISK_INDEX_PATH, "rb+");
        bank_fp = fopen(V_DISK_BANK_PATH, "rb+");
    }

    fseek(index_fp, 0, SEEK_SET);
    fseek(bank_fp, 0, SEEK_SET);

    memset(index_buf, 0, sizeof(index_buf));

    fread(index_buf, sizeof(char), V_DISK_INDEX_SIZE, index_fp);

    super_node_buf = (super_node*) index_buf;

    FFS_DBG_INFO("Image loading finished. \n");
    FFS_DBG_INFO("[ffs profile] total blocks = %d\n", super_node_buf->fs_blk_num);
    FFS_DBG_INFO("[ffs profile] total files = %d\n", super_node_buf->fs_inode_num);
    FFS_DBG_INFO("[ffs profile] root i_node in %d\n", super_node_buf->root_inode);

    i_node* test_ptr = (i_node*)(index_buf + super_node_buf->root_inode);

    FFS_DBG_INFO("[ffs config] root name is %s\n", test_ptr->file_name);
    FFS_DBG_INFO("[ffs config] super_node size = %d\n", sizeof(super_node));
    FFS_DBG_INFO("[ffs config] block size = %d\n", BLOCK_SIZE);
    FFS_DBG_INFO("[ffs config] i_node size = %d\n", sizeof(i_node));
}

void close_image() {
    fseek(index_fp, 0, SEEK_SET);
    fwrite(index_buf, sizeof(char), V_DISK_INDEX_SIZE, index_fp);
    fclose(index_fp);
    fclose(bank_fp);
    FFS_DBG_INFO("Write back to image file.\n");
}

i32 fetch_inode() {
    int i;
    for (i = 0; i < MAX_FILE_NUM; ++i) {
        if (super_node_buf->index_bitmap[i] == 0) {
            super_node_buf->index_bitmap[i] = 1; // it must be used
            super_node_buf->fs_inode_num++;
            return sizeof(super_node) + i * sizeof(i_node);
        }
    }
    FFS_DBG_ERR("No space to get free inode");
    return -ENOSPC;
}

i32 fetch_block() {
    int i;
    for (i = 0; i < BLOCK_NUM; ++i) {
        if (super_node_buf->bank_bitmap[i] == 0) {
            super_node_buf->bank_bitmap[i] = 1; // it must be used
            super_node_buf->fs_blk_num++;
            return i * BLOCK_SIZE;
        }
    }
    FFS_DBG_ERR("No space to get free block");
    return -ENOSPC;
}

i32 inode_ptr_to_off(i_node* ptr) {
    return (char*)(ptr) - index_buf;
}

i32 free_inode(i32 offset) {
    super_node_buf->index_bitmap[(offset - sizeof(super_node)) / sizeof(i_node)] = 0;
    super_node_buf->fs_inode_num--;
    return 0;
}

i32 free_block(i32 offset) {
    super_node_buf->bank_bitmap[offset / BLOCK_SIZE] = 0;
    super_node_buf->fs_blk_num--;
    return 0;
}

i32 read_one_block(i32 block_off, char* read_buf, size_t size, off_t off_in_blk) {
    if (off_in_blk + size > BLOCK_SIZE) {
        FFS_DBG_ERR("read bytes larger than a block");
        return -ENOSPC;
    }
    
    fseek(bank_fp, block_off + off_in_blk, SEEK_SET);
    fread(read_buf, size, 1, bank_fp);

    FFS_DBG_INFO("read from block: %s\n", read_buf);
    FFS_DBG_INFO("total_off = %d, size = %d, off_in_blk = %d\n", block_off+off_in_blk, size, off_in_blk);

    return 0;
}

i32 write_one_block(i32 block_off, const char* write_buf, size_t size, off_t off_in_blk) {
    if (off_in_blk + size > BLOCK_SIZE) {
        FFS_DBG_ERR("write bytes larger than a block");
        return -ENOSPC;
    }

    FFS_DBG_INFO("write to block: %s\n", write_buf);
    FFS_DBG_INFO("total_off = %d, size = %d, off_in_blk = %d\n", block_off+off_in_blk, size, off_in_blk);

    fseek(bank_fp, block_off + off_in_blk, SEEK_SET);
    fwrite(write_buf, size, 1, bank_fp);

    return 0;
}

i32 new_empty_inode(const char* file_name, char is_dir, i_node* inode_buf) {
    static int inode_id = 0;
    
    // fn
    strcpy(inode_buf->file_name, file_name);

    // list
    inode_buf->next_node = EMPTY;

    // stat
    // st_dev: ?
    // st_rdev: ?
    inode_buf->st.st_ino = inode_id++;

    if (is_dir) {
        inode_buf->first_son = EMPTY;
        inode_buf->st.st_mode = __S_IFDIR | 0777;
        inode_buf->st.st_size = 4096;
        inode_buf->st.st_nlink = 2; // default. two links?
    }
    else {
        inode_buf->first_son = NOT_DIRECTORY;
        inode_buf->st.st_mode = __S_IFREG | 0666;
        inode_buf->st.st_size = 0;
        inode_buf->st.st_nlink = 1; // default
    }

    inode_buf->st.st_uid = getuid();
    inode_buf->st.st_gid = getgid();

    inode_buf->st.st_blksize = BLOCK_SIZE;
    inode_buf->st.st_blocks = 0;

    inode_buf->st.st_atime = time(0);
    inode_buf->st.st_ctime = time(0);
    inode_buf->st.st_mtime = time(0);

    return 0;
}