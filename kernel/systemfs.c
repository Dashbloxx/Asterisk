/*
 *      dP      Asterisk is an operating system written fully in C and Intel-syntax
 *  8b. 88 .d8  assembly. It strives to be POSIX-compliant, and a faster & lightweight
 *   `8b88d8'   alternative to Linux for i386 processors.
 *   .8P88Y8.   
 *  8P' 88 `Y8  
 *      dP      
 *
 *  BSD 2-Clause License
 *  Copyright (c) 2017, ozkl, Nexuss
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *  
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "systemfs.h"
#include "common.h"
#include "fs.h"
#include "alloc.h"
#include "device.h"
#include "vmm.h"
#include "process.h"

static filesystem_node* g_systemfs_root = NULL;


static BOOL systemfs_open(File *file, uint32_t flags);
static filesystem_dirent *systemfs_readdir(filesystem_node *node, uint32_t index);
static filesystem_node *systemfs_finddir(filesystem_node *node, char *name);

static void create_nodes();

static filesystem_dirent g_dirent;

static int32_t systemfs_read_meminfo_totalpages(File *file, uint32_t size, uint8_t *buffer);
static int32_t systemfs_read_meminfo_usedpages(File *file, uint32_t size, uint8_t *buffer);
static BOOL systemfs_open_threads_dir(File *file, uint32_t flags);
static void systemfs_close_threads_dir(File *file);

void systemfs_initialize()
{
    g_systemfs_root = kmalloc(sizeof(filesystem_node));
    memset((uint8_t*)g_systemfs_root, 0, sizeof(filesystem_node));

    g_systemfs_root->node_type = FT_DIRECTORY;

    filesystem_node* root_fs = fs_get_root_node();

    fs_mkdir(root_fs, "system", 0);

    filesystem_node* system_node = fs_finddir(root_fs, "system");

    if (system_node)
    {
        system_node->node_type |= FT_MOUNT_POINT;
        system_node->mount_point = g_systemfs_root;
        g_systemfs_root->parent = system_node->parent;
        strcpy(g_systemfs_root->name, system_node->name);
    }
    else
    {
        PANIC("Could not create /system !");
    }

    g_systemfs_root->open = systemfs_open;
    g_systemfs_root->finddir = systemfs_finddir;
    g_systemfs_root->readdir = systemfs_readdir;

    create_nodes();
}

static void create_nodes()
{
    filesystem_node* node_mem_info = kmalloc(sizeof(filesystem_node));

    memset((uint8_t*)node_mem_info, 0, sizeof(filesystem_node));

    strcpy(node_mem_info->name, "meminfo");
    node_mem_info->node_type = FT_DIRECTORY;
    node_mem_info->open = systemfs_open;
    node_mem_info->finddir = systemfs_finddir;
    node_mem_info->readdir = systemfs_readdir;
    node_mem_info->parent = g_systemfs_root;

    g_systemfs_root->first_child = node_mem_info;

    filesystem_node* node_mem_info_total_pages = kmalloc(sizeof(filesystem_node));
    memset((uint8_t*)node_mem_info_total_pages, 0, sizeof(filesystem_node));
    strcpy(node_mem_info_total_pages->name, "totalpages");
    node_mem_info_total_pages->node_type = FT_FILE;
    node_mem_info_total_pages->open = systemfs_open;
    node_mem_info_total_pages->read = systemfs_read_meminfo_totalpages;
    node_mem_info_total_pages->parent = node_mem_info;

    node_mem_info->first_child = node_mem_info_total_pages;

    filesystem_node* node_mem_info_used_pages = kmalloc(sizeof(filesystem_node));
    memset((uint8_t*)node_mem_info_used_pages, 0, sizeof(filesystem_node));
    strcpy(node_mem_info_used_pages->name, "usedpages");
    node_mem_info_used_pages->node_type = FT_FILE;
    node_mem_info_used_pages->open = systemfs_open;
    node_mem_info_used_pages->read = systemfs_read_meminfo_usedpages;
    node_mem_info_used_pages->parent = node_mem_info;

    node_mem_info_total_pages->next_sibling = node_mem_info_used_pages;

    //

    filesystem_node* node_threads = kmalloc(sizeof(filesystem_node));
    memset((uint8_t*)node_threads, 0, sizeof(filesystem_node));

    strcpy(node_threads->name, "threads");
    node_threads->node_type = FT_DIRECTORY;
    node_threads->open = systemfs_open_threads_dir;
    node_threads->close = systemfs_close_threads_dir;
    node_threads->finddir = systemfs_finddir;
    node_threads->readdir = systemfs_readdir;
    node_threads->parent = g_systemfs_root;

    node_mem_info->next_sibling = node_threads;

    //

    filesystem_node* node_pipes = kmalloc(sizeof(filesystem_node));
    memset((uint8_t*)node_pipes, 0, sizeof(filesystem_node));

    strcpy(node_pipes->name, "pipes");
    node_pipes->node_type = FT_DIRECTORY;
    node_pipes->parent = g_systemfs_root;

    node_threads->next_sibling = node_pipes;

    //

    filesystem_node* node_shm = kmalloc(sizeof(filesystem_node));
    memset((uint8_t*)node_shm, 0, sizeof(filesystem_node));

    strcpy(node_shm->name, "shm");
    node_shm->node_type = FT_DIRECTORY;
    node_shm->parent = g_systemfs_root;

    node_pipes->next_sibling = node_shm;
}

static BOOL systemfs_open(File *file, uint32_t flags)
{
    return TRUE;
}

static filesystem_dirent *systemfs_readdir(filesystem_node *node, uint32_t index)
{
    int counter = 0;

    filesystem_node* child = node->first_child;

    //Screen_PrintF("systemfs_readdir-main:%s index:%d\n", node->name, index);

    while (NULL != child)
    {
        //Screen_PrintF("systemfs_readdir-child:%s\n", child->name);
        if (counter == index)
        {
            strcpy(g_dirent.name, child->name);
            g_dirent.file_type = child->node_type;

            return &g_dirent;
        }

        ++counter;

        child = child->next_sibling;
    }

    return NULL;
}

static filesystem_node *systemfs_finddir(filesystem_node *node, char *name)
{
    filesystem_node* child = node->first_child;
    while (NULL != child)
    {
        if (strcmp(name, child->name) == 0)
        {
            return child;
        }

        child = child->next_sibling;
    }

    return NULL;
}

static int32_t systemfs_read_meminfo_totalpages(File *file, uint32_t size, uint8_t *buffer)
{
    if (size >= 4)
    {
        if (file->offset == 0)
        {
            int total_pages = vmm_get_total_page_count();

            sprintf((char*)buffer, size, "%d", total_pages);

            int len = strlen((char*)buffer);

            file->offset += len;

            return len;
        }
        else
        {
            return 0;
        }
    }
    return -1;
}

static int32_t systemfs_read_meminfo_usedpages(File *file, uint32_t size, uint8_t *buffer)
{
    if (size >= 4)
    {
        if (file->offset == 0)
        {
            int usedPages = vmm_get_used_page_count();

            sprintf((char*)buffer, size, "%d", usedPages);

            int len = strlen((char*)buffer);

            file->offset += len;

            return len;
        }
        else
        {
            return 0;
        }
    }
    return -1;
}

static BOOL systemfs_open_thread_file(File *file, uint32_t flags)
{
    return TRUE;
}

static void systemfs_close_thread_file(File *file)
{

}

static int32_t systemfs_read_thread_file(File *file, uint32_t size, uint8_t *buffer)
{
    if (size >= 128)
    {
        if (file->offset == 0)
        {
            int thread_id = atoi(file->node->name);
            Thread* thread = thread_get_by_id(thread_id);
            if (thread)
            {
                uint32_t char_index = 0;
                char_index += sprintf((char*)buffer + char_index, size - char_index, "tid:%d\n", thread->threadId);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "birthTime:%d\n", thread->birth_time);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "userMode:%d\n", thread->user_mode);
                uint8_t state[10];
                thread_state_to_string(thread->state, state, 10);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "state:%s\n", state);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "syscalls:%d\n", thread->called_syscall_count);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "contextSwitches:%d\n", thread->context_switch_count);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "cpuTime:%d\n", thread->consumed_cpu_time_ms);
                char_index += sprintf((char*)buffer + char_index, size - char_index, "cpuUsage:%d\n", thread->usage_cpu);
                if (thread->owner)
                {
                    char_index += sprintf((char*)buffer + char_index, size - char_index, "process:%d (%s)\n", thread->owner->pid, thread->owner->name);
                }
                else
                {
                    char_index += sprintf((char*)buffer + char_index, size - char_index, "process:-\n");
                }

                int len = char_index;

                file->offset += len;

                return len;
            }
        }
        else
        {
            return 0;
        }
    }
    return -1;
}

static void clean_thread_nodes(File *file)
{
    filesystem_node* node = file->node->first_child;

    while (node)
    {
        filesystem_node* next = node->next_sibling;

        kfree(node);

        node = next;
    }
}

static BOOL systemfs_open_threads_dir(File *file, uint32_t flags)
{
    char buffer[16];

    clean_thread_nodes(file);

    //And fill again

    filesystem_node* node_previous = NULL;

    Thread* thread = thread_get_first();

    while (NULL != thread)
    {
        filesystem_node* node_thread = kmalloc(sizeof(filesystem_node));
        memset((uint8_t*)node_thread, 0, sizeof(filesystem_node));

        sprintf(buffer, 16, "%d", thread->threadId);

        strcpy(node_thread->name, buffer);
        node_thread->node_type = FT_FILE;
        node_thread->open = systemfs_open_thread_file;
        node_thread->close = systemfs_close_thread_file;
        node_thread->read = systemfs_read_thread_file;
        node_thread->finddir = systemfs_finddir;
        node_thread->readdir = systemfs_readdir;
        node_thread->parent = file->node;

        if (node_previous)
        {
            node_previous->next_sibling = node_thread;
        }
        else
        {
            file->node->first_child = node_thread;
        }

        node_previous = node_thread;
        thread = thread->next;
    }



    return TRUE;
}

static void systemfs_close_threads_dir(File *file)
{
    //left blank intentionally
}
