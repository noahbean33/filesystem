#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

char dir_list[ 256 ][ 256 ];
int curr_dir_idx = -1;

char files_list[ 256 ][ 256 ];
int curr_file_idx = -1;

char files_content[ 256 ][ 256 ];
int curr_file_content_idx = -1;

void add_dir( const char *dir_name )
{
	if ( curr_dir_idx >= 255 )
		return;
	
	curr_dir_idx++;
	strncpy( dir_list[ curr_dir_idx ], dir_name, 255 );
	dir_list[ curr_dir_idx ][ 255 ] = '\0';
}

int is_dir( const char *path )
{
	path++; // Eliminating "/" in the path
	
	for ( int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++ )
		if ( strcmp( path, dir_list[ curr_idx ] ) == 0 )
			return 1;
	
	return 0;
}

void add_file( const char *filename )
{
	if ( curr_file_idx >= 255 )
		return;
	
	curr_file_idx++;
	strncpy( files_list[ curr_file_idx ], filename, 255 );
	files_list[ curr_file_idx ][ 255 ] = '\0';
	
	curr_file_content_idx++;
	files_content[ curr_file_content_idx ][ 0 ] = '\0';
}

int is_file( const char *path )
{
	path++; // Eliminating "/" in the path
	
	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
		if ( strcmp( path, files_list[ curr_idx ] ) == 0 )
			return 1;
	
	return 0;
}

int get_file_index( const char *path )
{
	path++; // Eliminating "/" in the path
	
	for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
		if ( strcmp( path, files_list[ curr_idx ] ) == 0 )
			return curr_idx;
	
	return -1;
}

void write_to_file( const char *path, const char *new_content )
{
	int file_idx = get_file_index( path );
	
	if ( file_idx == -1 ) // No such file
		return;
		
	strncpy( files_content[ file_idx ], new_content, 255 ); 
	files_content[ file_idx ][ 255 ] = '\0';
}

// ... //

static int do_getattr( const char *path, struct stat *st )
{
	st->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
	st->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
	st->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
	st->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now
	
	if ( strcmp( path, "/" ) == 0 || is_dir( path ) == 1 )
	{
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
	}
	else if ( is_file( path ) == 1 )
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		int file_idx = get_file_index( path );
		if ( file_idx != -1 )
			st->st_size = strlen( files_content[ file_idx ] );
		else
			st->st_size = 0;
	}
	else
	{
		return -ENOENT;
	}
	
	return 0;
}

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory
	
	if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
	{
		for ( int curr_idx = 0; curr_idx <= curr_dir_idx; curr_idx++ )
			filler( buffer, dir_list[ curr_idx ], NULL, 0 );
	
		for ( int curr_idx = 0; curr_idx <= curr_file_idx; curr_idx++ )
			filler( buffer, files_list[ curr_idx ], NULL, 0 );
	}
	
	return 0;
}

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
	int file_idx = get_file_index( path );
	
	if ( file_idx == -1 )
		return -ENOENT;
	
	char *content = files_content[ file_idx ];
	int len = strlen( content );
	
	if ( offset >= len )
		return 0;
	
	int bytes_to_read = len - offset;
	if ( bytes_to_read > size )
		bytes_to_read = size;
	
	memcpy( buffer, content + offset, bytes_to_read );
		
	return bytes_to_read;
}

static int do_mkdir( const char *path, mode_t mode )
{
	path++;
	add_dir( path );
	
	return 0;
}

static int do_mknod( const char *path, mode_t mode, dev_t rdev )
{
	path++;
	add_file( path );
	
	return 0;
}

static int do_write( const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info )
{
	int file_idx = get_file_index( path );
	
	if ( file_idx == -1 )
		return -ENOENT;
	
	int current_len = strlen( files_content[ file_idx ] );
	int new_len = offset + size;
	
	if ( new_len > 255 )
		new_len = 255;
	
	if ( offset > current_len )
	{
		memset( files_content[ file_idx ] + current_len, 0, offset - current_len );
	}
	
	int bytes_to_write = new_len - offset;
	if ( bytes_to_write > 0 )
	{
		memcpy( files_content[ file_idx ] + offset, buffer, bytes_to_write );
		files_content[ file_idx ][ new_len ] = '\0';
	}
	
	return bytes_to_write > 0 ? bytes_to_write : 0;
}

static struct fuse_operations operations = {
    .getattr	= do_getattr,
    .readdir	= do_readdir,
    .read		= do_read,
    .mkdir		= do_mkdir,
    .mknod		= do_mknod,
    .write		= do_write,
};

int main( int argc, char *argv[] )
{
	return fuse_main( argc, argv, &operations, NULL );
}
