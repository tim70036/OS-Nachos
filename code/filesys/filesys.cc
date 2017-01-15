// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include <string.h>

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1


//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
    DEBUG(dbgFile, "Initializing the file system.");
    if (format) {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
		FileHeader *mapHdr = new FileHeader;
		FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

		// First, allocate space for FileHeaders for the directory and bitmap
		// (make sure no one else grabs these!)
		freeMap->Mark(FreeMapSector);
		freeMap->Mark(DirectorySector);

		// Second, allocate space for the data blocks containing the contents
		// of the directory and bitmap files.  There better be enough space!

		ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
		ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

		// Flush the bitmap and directory FileHeaders back to disk
		// We need to do this before we can "Open" the file, since open
		// reads the file header off of disk (and currently the disk has garbage
		// on it!).

        DEBUG(dbgFile, "Writing headers back to disk.");
		mapHdr->WriteBack(FreeMapSector);
		dirHdr->WriteBack(DirectorySector);

		// OK to open the bitmap and directory files now
		// The file system operations assume these two files are left open
		// while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);

		// Once we have the files "open", we can write the initial version
		// of each file back to disk.  The directory at this point is completely
		// empty; but the bitmap has been changed to reflect the fact that
		// sectors on the disk have been allocated for the file headers and
		// to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
		freeMap->WriteBack(freeMapFile);	 // flush changes to disk
		directory->WriteBack(directoryFile);

		if (debug->IsEnabled('f')) {
			freeMap->Print();
			directory->Print();
        }
        delete freeMap;
		delete directory;
		delete mapHdr;
		delete dirHdr;
    } else {
		// if we are not formatting the disk, just open the files representing
		// the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
	delete freeMapFile;
	delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(char *pathName, int initialSize, bool isDir)
{
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    /* MP4 */ /* DirectoryFileSize */
    if(isDir)   initialSize = DirectoryFileSize;

    printf("Creating file path [%s]   size [%d bytes] \n",pathName,initialSize);

    /* MP4 */
    /* Find the directory containing the target file */
    char name[1000];
    strcpy(name, pathName); /* prevent pathName being modified */
    OpenFile* curDirFile = FindSubDirectory(name); /* name will be cut to file name */
    if(curDirFile == NULL)  return FALSE; /* Directory file not found */
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(curDirFile);


    printf("Find desired directory.\n");
    printf("Start creating [%s]\n\n",name);
    DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);

    if (directory->Find(name) != -1)
      success = FALSE;			// file is already in directory
    else {
        freeMap = new PersistentBitmap(freeMapFile,NumSectors);
        sector = freeMap->FindAndSet();	// find a sector to hold the file header
    	if (sector == -1)
            success = FALSE;		// no free block for file header
        else if (!directory->Add(name, sector, isDir)) /* MP4 */ /* isDir */
            success = FALSE;	// no space in directory
	    else
        {
        	hdr = new FileHeader;

            /* MP4 */
            /* Calculate the total size of all headers for this file */
            int totalSize = hdr->Allocate(freeMap, initialSize);

    	    if (totalSize <= 0)
                	success = FALSE;	// no space on disk for data
    	    else
            {
                    /* Output the totalSize */
                    printf("Successfully allocated, total headers' size [%d bytes]\n",totalSize);

                    success = TRUE;
    		        // everthing worked, flush all changes back to disk
        	    	hdr->WriteBack(sector);
        	    	directory->WriteBack(curDirFile); /* MP4 */ /* write back to dir */
        	    	freeMap->WriteBack(freeMapFile);

                    /* MP4 */
                    /* If it's a new dir, need initialize */
                    // if(isDir)
                    // {
                    //     OpenFile* tmpFile = new OpenFile(sector);
                    //     Directory* tmpDir = new Directory(NumDirEntries);
                    //     tmpDir->WriteBack(tmpFile);
                    //     delete tmpDir;
                    //     delete tmpFile;
                    // }
	        }
            delete hdr;
	    }
        delete freeMap;
    }

    /* MP4 */
    /* Don't delete root directoryFile */
    if(curDirFile != directoryFile && curDirFile != NULL) delete curDirFile;

    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *pathName)
{
    OpenFile *openFile = NULL;
    int sector;

    /* MP4 */
    /* Find the directory containing the target file */
    char name[1000];
    strcpy(name, pathName); /* prevent pathName being modified */
    OpenFile* curDirFile = FindSubDirectory(name);
    if(curDirFile == NULL)  return NULL; /* Directory file not found , return NULL */
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(curDirFile);

    printf("Opening File [%s]\n\n",name);

    DEBUG(dbgFile, "Opening file" << name);
    sector = directory->Find(name);

    /* MP4 */
    /* Too much open file? */
    if (openFileTableTop >= 487) return NULL;
    /* We find the file header? */
    if (sector >= 0)
    {
        openFile = new OpenFile(sector);	// name was found in directory
        openFileTable[openFileTableTop++] = openFile;
    }

    /* MP4 */
    /* Don't delete root directoryFile */
    if(curDirFile != directoryFile && curDirFile != NULL) delete curDirFile;

    delete directory;
    return openFile;				// return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(bool recursive, char *pathName)
{
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;

    /* MP4 */
    /* Find the directory containing the target file */
    char name[1000];
    strcpy(name, pathName); /* prevent pathName being modified */
    OpenFile* curDirFile = FindSubDirectory(name);
    if(curDirFile == NULL)  return FALSE; /* Directory file not found */
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(curDirFile);

    printf("Removing File [%s]\n\n",name);

    sector = directory->Find(name);
    // file not found
    if (sector == -1)
    {
        /* MP4 */
        /* Don't delete root directoryFile */
        if(curDirFile != directoryFile && curDirFile != NULL) delete curDirFile;

        delete directory;
        return FALSE;
    }

    /* MP4 */
    /* Recursively remove , only if this file is a dir */
    if(directory->isDir(name) && recursive)
    {
        /* Read out the target dir */
        OpenFile* tarOpenFile = new OpenFile(sector);
        Directory* tarDirectory = new Directory(NumDirEntries);
        tarDirectory->FetchFrom(tarOpenFile);

        /* Creating Target's Path Name */
        char tarPathName[1000];
        strcpy(tarPathName, pathName); /* pathName: /a/b/c */
        int offset = strlen(tarPathName);
        tarPathName[offset] = '/'; /* tarPathName : /a/b/c/ */

        /* Remove all things in the target directory */
        for(int i=0 ; i<tarDirectory->tableSize ; i++)
        {
            if(tarDirectory->table[i].inUse)
            {
                strcpy(tarPathName+offset+1, tarDirectory->table[i].name); /* Append the file name */
                Remove(TRUE, tarPathName);
            }
        }

        delete tarOpenFile;
        delete tarDirectory;
    }

    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new PersistentBitmap(freeMapFile,NumSectors);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(curDirFile); /* MP4 write to curDirFile */

    /* MP4 */
    /* Don't delete root directoryFile */
    if(curDirFile != directoryFile && curDirFile != NULL) delete curDirFile;

    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List(bool recursive, char* listDirectoryPathName)
{
    /* MP4 */
    /* Special case : list root dir */
    if(strcmp(listDirectoryPathName, "/") == 0)
    {
        printf("Listing dir [/]...\n\n");
        Directory *directory = new Directory(NumDirEntries);
        directory->FetchFrom(directoryFile);
        directory->List(recursive,0);
        delete directory;
        return;
    }

    /* MP4 */
    /* Find the directory containing the target dir */
    char listDirectoryName[1000];
    strcpy(listDirectoryName, listDirectoryPathName); /* prevent pathName being modified */
    OpenFile* curDirFile = FindSubDirectory(listDirectoryName);
    if(curDirFile == NULL)  return; /* Directory file not found */
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(curDirFile);


    printf("Listing dir [%s]...\n\n",listDirectoryName);

    /* Find the target dir */
    int sector = directory->Find(listDirectoryName);
    if (sector >= 0)
    {
        OpenFile* tmpFile = new OpenFile(sector);
        Directory* tmpDir = new Directory(NumDirEntries);
        tmpDir->FetchFrom(tmpFile);
        tmpDir->List(recursive,0);
        delete tmpDir;
        delete tmpFile;
    }

    /* MP4 */
    /* Don't delete root directoryFile */
    if(curDirFile != directoryFile && curDirFile != NULL) delete curDirFile;
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile,NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}


/* MP4 */
/* Find the sub-directory */
OpenFile* FileSystem::FindSubDirectory(char* name)
{
    Directory *curDirectory = new Directory(NumDirEntries);
    OpenFile*  curDirFile = directoryFile; /* root file */
    curDirectory->FetchFrom(directoryFile);

    //printf("Current Dir:\n");
    //curDirectory->List(FALSE, 0);
    //printf("\nStart finding sub-directory\n");
    char* cut = strtok(name, "/");


    if(cut == NULL)
    {
        delete curDirectory;
        return NULL;
    }
    //printf("The first cut : %s\n", cut);

    while(1)
    {
        char *nextCut = strtok(NULL, "/");
        if(nextCut != NULL) /* Go deeper */
        {
            //printf("Next cut exist(%s), go deeper\n", nextCut);
            /* Does sub-directory exist? */
            int sector = curDirectory->Find(cut);
            if(sector == -1)
            {
                printf("Sub-directory %s not found, return NULL\n\n",cut);
                delete curDirectory;
                if(curDirFile != directoryFile) delete curDirFile; /* Don;t del root file */
                return NULL;
            }

            /* Deeper !!!! */
            if(curDirFile != directoryFile) delete curDirFile; /* Don;t del root file */
            curDirFile = new OpenFile(sector);
            curDirectory->FetchFrom(curDirFile);
            printf("Change dir to %s\n",cut);
            //printf("Current Dir:\n");
            //curDirectory->List(FALSE, 0);
            cut = nextCut;
        }
        /* In the end */
        else
        {
            //printf("Next cut doesn't exist, stop \n");
            strcpy(name,cut); /* name will be modified to file name */
            delete curDirectory;
            return curDirFile;
        }
    }
}

#endif // FILESYS_STUB
