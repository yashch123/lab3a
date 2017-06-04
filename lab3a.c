#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "ext2_fs.h"

static unsigned int block_size = 0;
struct ext2_inode inode;
int fd;

void read_dir(int i,int parent_inode_num,int offset, int blocks);

void indirEntry(int i, unsigned int parent_inode_num, int offset, char call, int blocks, int last_block)
{
  int num=i;
  
  int k=0,ctr=0;
  if(i==EXT2_IND_BLOCK)
    {
      
         
      if(inode.i_block[i]==0)
	return;

      __u32 *arr=calloc(block_size,4);

    
    
      ctr= pread(fd,arr,block_size,1024+((offset-1)*block_size));
     
      for(k=0;k<block_size;k++)
	{
	  if(arr[k]==0)
	    {
	      num+=1;
	      continue;
	    }
	  
	  if(call=='d')
	    read_dir(0,parent_inode_num,arr[k],blocks);
	  
	
	  //if(call=='i')
	  
	  fprintf(stdout,"INDIRECT,%d,1,%d,%d,%d\n",parent_inode_num,num,last_block,arr[k]);
	    num+=1;

	}
    }

  else if(i==EXT2_DIND_BLOCK)
    {
      if(inode.i_block[i]==0)
	return;

      int num1;
      __u32 *arr1=calloc(block_size,4);
      pread(fd,arr1,block_size,1024+((offset-1)*block_size));
      
      for(k=0;k<block_size;k++)
	{
	  if(arr1[k]==0)
	     continue;
	  num1=12+256+256*k;
	  fprintf(stdout,"INDIRECT,%d,2,%d,%d,%d\n",parent_inode_num,num1,last_block,arr1[k]);
	  indirEntry(12,parent_inode_num,arr1[k],'i',blocks,arr1[k]);  //Recursive call!
	}
    }
   else if(i==EXT2_TIND_BLOCK)
     {
       
       if(inode.i_block[i]==0)
	return;
       int num2;
       __u32 *arr2=calloc(block_size,4);
      pread(fd,arr2,block_size,1024+((inode.i_block[i]-1)*block_size));

      for(k=0;k<block_size;k++)
	{
	   if(arr2[k]==0)
	     continue;
	   num2=12+256+(256*256*k);
	   fprintf(stdout,"INDIRECT,%d,3,%d,%d,%d\n",parent_inode_num,num2,inode.i_block[i],arr2[k]);
	   indirEntry(13,parent_inode_num,arr2[k],'i',blocks,arr2[k]);  //Recursive call!
	}

       
     }
}
  
void read_dir(int i,int parent_inode_num,int offset,int blocks)
{
  int counter;
  struct ext2_dir_entry dir_entries;
  if(i==12)
    {
      indirEntry(12,parent_inode_num,offset,'d',blocks,inode.i_block[i]);
    }
  else
    {
      
      counter = 0;
      while(counter < block_size)
	{
	  //Now we can start reading the directory entries:
	  pread(fd,&dir_entries,sizeof(struct ext2_dir_entry),1024+((offset-1)*block_size)+counter);
	  
	  
	  if(dir_entries.rec_len==0) //causes infinite loop otherwise
	    {
	      break;    //CHECKK:DO WE NEED TO FPRINTF HERE????
	    }
      
	  if(dir_entries.inode==0) //CHECK SPEC AGAIN (print only if its valid)
	    {      
	      counter = counter + dir_entries.rec_len;
	      continue;
	    }
	  char file_name[EXT2_NAME_LEN+1];
	  memcpy(file_name, dir_entries.name, dir_entries.name_len);
	  file_name[dir_entries.name_len] = 0;
	  
	  fprintf(stdout,"DIRENT,%d,%d,%d,%d,%d,'%s'\n",parent_inode_num,counter,dir_entries.inode,dir_entries.rec_len,dir_entries.name_len, file_name);
	  
	  
      counter=counter+dir_entries.rec_len;
      
	}
    }
}



void dirEntry(unsigned int parent_inode_num,int blocks)
{
  int i;
  
  for(i=0;i<13;i++)
    {
      if(inode.i_block[i]!=0)
	read_dir(i,parent_inode_num,inode.i_block[i],blocks);
    }
  
  
}


int main(int argc, char **argv)
{
  char* file;
  if(argc!=2)
    {
      fprintf(stderr,"Bad input as argument! Usage: ./lab3a [fileName].img\n");
      exit(1);
    }
  else
    file=argv[1];
      
  /***********************************SUPERBOCK CODE********************************************/

  struct ext2_super_block super;
  
  
  /* open floppy device */
  
  if ((fd = open(file, O_RDONLY)) < 0) {
    fprintf(stderr,"File sent in as argument could not be opened!");
    exit(1);  
  }
  
  /* read super-block */
  
  if(pread(fd,&super,sizeof(super),1024)<0)
    {
      fprintf(stderr, "Unable to read from given file system!\n");
      exit(1);
    }
  
  if (super.s_magic != EXT2_SUPER_MAGIC) {
    fprintf(stderr, "Not a Ext2 filesystem\n");
    exit(1);
  }
  
  block_size = 1024 << super.s_log_block_size;
  
  fprintf(stdout,"SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", super.s_blocks_count, super.s_inodes_count, block_size, super.s_inode_size, super.s_blocks_per_group, super.s_inodes_per_group, super.s_first_ino);

  /*************************************GROUP DESCRIPTOR CODE************************************/

  struct ext2_group_desc group;

  if(pread(fd,&group,sizeof(group),1024+block_size)<0)
    {
      fprintf(stderr, "Unable to read from given file system!\n");
      exit(1);
    } 

  fprintf(stdout,"GROUP,0,%d,%d,%d,%d,%d,%d,%d\n", super.s_blocks_count, super.s_inodes_count, group.bg_free_blocks_count, group.bg_free_inodes_count, group.bg_block_bitmap, group.bg_inode_bitmap, group.bg_inode_table);  

  /**************************************BFREE CODE***********************************************/

  unsigned char* block_map;
  block_map=(unsigned char*)malloc(block_size);
  int b_num=0;

  if(pread(fd,block_map, block_size, 1024+(group.bg_block_bitmap-1)*block_size)<0)
    {
      fprintf(stderr, "Unable to read from given file system!\n");
      exit(1);
    }

  unsigned char *temp=(unsigned char *)malloc(sizeof(unsigned char));
  int i=0,j=0;
 
   for(i=0;i<block_size;i++)
    {
     
      temp=&block_map[i];
      for(j=0;j<8;j++)
	{
	  b_num++;
	  if(b_num>super.s_blocks_count)
	    break;
	  unsigned int test = 1 << j;
	  
	  if((*temp&test)==0)
	    fprintf(stdout,"BFREE,%d\n", 8*i+j+1);
	  }
      }
  
  /**************************************IFREE CODE***********************************************/
   unsigned char* inode_map;
   //memset((unsigned char*)block_map,-1,1024);   //Resetting the char buffer to now hold Inode Bitmap
   int i_num=0;
   inode_map=(unsigned char*)malloc(1024);

   
   if(pread(fd,inode_map, block_size, 1024+(group.bg_inode_bitmap-1)*block_size)<0)
    {
      fprintf(stderr, "Unable to read from given file system!\n");
      exit(1);
    }

   unsigned char *temp2=(unsigned char *)malloc(sizeof(unsigned char));
  
    for(i=0;i<block_size;i++)
    {
      // fprintf(stderr,"%d\n",inode_map[i]);
      temp2=&inode_map[i];
      for(j=0;j<8;j++)
	{
	  i_num++;
	  if(i_num>super.s_inodes_count)
	    break;
	  unsigned int test2 = 1 << j;
	  
	  if((*temp2&test2)==0)
	    fprintf(stdout,"IFREE,%d\n", 8*i+j+1);
	 
	}
    }
      
    /************************************INODE TABLE*********************************/
    
    /* number of inodes per block */
    unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);

    /* size in blocks of the inode table */
    unsigned int iblock_size = super.s_inodes_per_group / inodes_per_block;
    
   
    //    i_table=(struct ext2_inode *)malloc(block_size*i_blocks);
    int inode_no;
    int reg=0,dir=0;
    char ch='?';
    
    if(pread(fd,inode_map, block_size, 1024+(group.bg_inode_bitmap-1)*block_size)<0)
      {
	fprintf(stderr, "Unable to read from given file system!\n");
	exit(1);
      }

    i_num=0;
    
    for(i=0;i<super.s_inodes_per_group;i++)
      {

	temp2=&inode_map[i];
	for(j=0;j<8;j++)
	  {
	    i_num++;
	    if(i_num>super.s_inodes_count)
	      break;
	    unsigned int test3 = 1 << j;
	    if(!((*temp2)&test3))
	      continue;
	    else
	      {
		inode_no=8*i+j+1;
		if(pread(fd,&inode,sizeof(inode), 1024+((inode_no-1)*sizeof(struct ext2_inode))+((group.bg_inode_table-1)*block_size))<0)
		  {
		    fprintf(stderr, "Unable to read from given file system!\n");
		    exit(1);
		  }
		
		if(inode.i_mode!=0 && inode.i_links_count!=0)
		  {

		     if(S_ISREG(inode.i_mode))
		      ch='f';
		    else if(S_ISDIR(inode.i_mode))
		      ch='d';
		    else if(S_ISLNK(inode.i_mode))
		      ch='s';
		     
		    time_t c_time, a_time,m_time;
		    struct tm *a,*c,*m;

		    fprintf(stdout,"INODE,%d,%c,%o,%d,%d,%d,",8*i+j+1,ch,inode.i_mode & 4095,inode.i_uid,inode.i_gid,inode.i_links_count);
		    c_time=inode.i_ctime;
		    a_time=inode.i_atime;
		    m_time=inode.i_mtime;
		    
		   
		   
		   
		   
		    c=gmtime(&c_time);
		    if(c->tm_year>=100)
		      c->tm_year-=100;
		   
		    fprintf(stdout,"%02d/%02d/%02d %02d:%02d:%02d,",c->tm_mon+1,c->tm_mday,c->tm_year, c->tm_hour, c->tm_min, c->tm_sec);

		    m=gmtime(&m_time);
		    if(m->tm_year>=100)
		      m->tm_year-=100;
		   
		    fprintf(stdout,"%02d/%02d/%02d %02d:%02d:%02d,",m->tm_mon+1,m->tm_mday,m->tm_year, m->tm_hour, m->tm_min, m->tm_sec);
		    a=gmtime(&a_time);
		    if(a->tm_year>=100)
		      a->tm_year-=100;
		    fprintf(stdout,"%02d/%02d/%02d %02d:%02d:%02d,",a->tm_mon+1,a->tm_mday,a->tm_year, a->tm_hour, a->tm_min, a->tm_sec);
		   

		    fprintf(stdout,"%d,%d",inode.i_size,inode.i_blocks);
		    int k;
		    for(k=0;k<15;k++)
		      fprintf(stdout,",%d",inode.i_block[k]);

		   
		      fprintf(stdout,"\n");

		      if(!S_ISDIR(inode.i_mode)){
			indirEntry(12,inode_no,inode.i_block[12],'i',super.s_blocks_count,inode.i_block[12]);
			indirEntry(13,inode_no,inode.i_block[13],'i',super.s_blocks_count,inode.i_block[13]);
			indirEntry(14,inode_no,inode.i_block[14],'i',super.s_blocks_count,inode.i_block[14]);
		      }

		     if(S_ISDIR(inode.i_mode))
		      {
			dirEntry(inode_no,super.s_blocks_count);
		      }
		      // indirEntry(fd,inode,inode_no);
		  }
	      }
	  }
      }
	    
    /******************************DIRECTORY*********************************/

    
    
    
   
  exit(0);
    
}
