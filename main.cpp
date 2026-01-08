//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2020-12-01, rev. 2025-12-04
//
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.

#include <iostream>
#include <string>
#include <random>
#include <memory>
#include <vector>

//
#include <cstdio>
#include <cstdlib>
#include <ctime>
//
#include "gl_frontEnd.h"
#include <thread>
#include <unistd.h>
#include <mutex>



//	feel free to "un-use" std if this is against your beliefs.
using namespace std;

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Private Functions' Prototypes
//-----------------------------------------------------------------------------
#endif

void initializeApplication(void);
void cleanupAndQuit();
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = Direction::NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd);
void generateWalls(void);
void generatePartitions(void);

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Application-level Global Variables
//-----------------------------------------------------------------------------
#endif

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType** grid;
unsigned int numRows = 0;			//	height of the grid
unsigned int numCols = 0;			//	width
//	The number of traveler threads (argument to the program)
unsigned int numTravelers = 0;		//	initial number = numTravelersDone + numLiveThreads
unsigned int numTravelersDone = 0;
unsigned int numLiveThreads = 0;	//	the number of live traveler threads
//
GridPosition exitPos;				//	location of the exit (randomly generated)
GLfloat** travelerColor;			//	unique colors assigned to the travelers
mutex globalMutex;
// V5: one lock per grid square
std::mutex** gridLocks;



//
vector<shared_ptr<Traveler> > travelerList;
vector<shared_ptr<SlidingPartition> > partitionList;

GLint refreshMillisecs = 15;			//	number of milliseconds between screen refreshes

//	travelers' sleep time between moves (in microseconds).  Feel free to adjust
const int MIN_SLEEP_TIME = 1000;
int travelerSleepTime = 100000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
time_t launchTime;

//---------------------------
//	Random generators
//---------------------------
const unsigned int MAX_NUM_INITIAL_SEGMENTS = 8;
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> unsignedNumberGenerator(0, numeric_limits<unsigned int>::max());
uniform_int_distribution<unsigned int> segmentNumberGenerator(0, MAX_NUM_INITIAL_SEGMENTS);
uniform_int_distribution<unsigned int> segmentDirectionGenerator(0, static_cast<unsigned int>(Direction::NUM_DIRECTIONS)-1);
//
//	This will produce a random bool value true/false with 50/50 equal probability.
bernoulli_distribution headsOrTails(0.5);
//	If we want a biased coin producing "true" 70% of the time, we would declare
//bernoulli_distribution headsOrTails(0.7);
//
//	We declare the distributions here because we need them to be global, accessible
//	from different functions, but we will only know the range they must cover after
//	we have read the dimensions of the grid from the argument list.
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;

bool trySlidePartition(shared_ptr<SlidingPartition> part, Direction dir)
{

vector<unique_lock<mutex>> locks;

	// lock every grid square used by the partition
	for (auto& pos : part->blockList)
	{
		locks.emplace_back(gridLocks[pos.row][pos.col]);
	}


    int dr = 0, dc = 0;
    // Decide how the partition moves based on direction
    if (dir == Direction::NORTH) dr = 1;
    if (dir == Direction::SOUTH) dr = -1;
    if (dir == Direction::WEST)  dc = 1;
    if (dir == Direction::EAST)  dc = -1;

    // check if all blocks can move
    for (auto& pos : part->blockList)
    {
        int nr = pos.row + dr;
        int nc = pos.col + dc;

        if (nr < 0 || nr >= (int)numRows ||
            nc < 0 || nc >= (int)numCols)
            return false;

        if (grid[nr][nc] != SquareType::FREE_SQUARE)
            return false;
    }

    // clear old positions
    for (auto& pos : part->blockList)
        grid[pos.row][pos.col] = SquareType::FREE_SQUARE;

    // move blocks
    for (auto& pos : part->blockList)
    {
        pos.row += dr;
        pos.col += dc;
        grid[pos.row][pos.col] =
            part->isVertical ? SquareType::VERTICAL_PARTITION
                             : SquareType::HORIZONTAL_PARTITION;
    }

    return true;
}



void travelerThread(shared_ptr<Traveler> traveler)
{
    {
        // count this thread as running
        lock_guard<mutex> glock(globalMutex);
        numLiveThreads++;
    }

    while (true)
    {
        usleep(travelerSleepTime);

        Direction dir;
        int newRow, newCol;

		
        {
            lock_guard<mutex> tlock(traveler->travelerMutex);
            TravelerSegment& head = traveler->segmentList[0];

            dir = newDirection();
            newRow = head.row;
            newCol = head.col;

            if (dir == Direction::NORTH) newRow++;
            if (dir == Direction::SOUTH) newRow--;
            if (dir == Direction::WEST)  newCol++;
            if (dir == Direction::EAST)  newCol--;
        }


		
        if (newRow < 0 || newRow >= (int)numRows ||
            newCol < 0 || newCol >= (int)numCols)
            continue;

        SquareType targetSquare;

        {
            lock_guard<mutex> cellLock(gridLocks[newRow][newCol]);

            targetSquare = grid[newRow][newCol];

            if (targetSquare == SquareType::WALL)
                continue;

		if (targetSquare == SquareType::EXIT)
		{
			// EC 4.1
			while (true)
			{
				{
					// lock traveler first, then grid squares
					lock_guard<mutex> tlock(traveler->travelerMutex);

					if (traveler->segmentList.size() <= 1)
						break;

					// remove last segment
					TravelerSegment tail = traveler->segmentList.back();
					traveler->segmentList.pop_back();

					// clear grid square of removed segment
					lock_guard<mutex> cellLock(gridLocks[tail.row][tail.col]);
					grid[tail.row][tail.col] = SquareType::FREE_SQUARE;
				}

				// slow fade out so that its visible
				usleep(travelerSleepTime);
			}

			//  remove head
			{
				lock_guard<mutex> tlock(traveler->travelerMutex);
				TravelerSegment& head = traveler->segmentList[0];

				lock_guard<mutex> cellLock(gridLocks[head.row][head.col]);
				grid[head.row][head.col] = SquareType::FREE_SQUARE;
			}

			// mark traveler done
			{
				lock_guard<mutex> glock(globalMutex);
				numTravelersDone++;
			}

			break;
		}

        }

		
        if (targetSquare == SquareType::VERTICAL_PARTITION ||
            targetSquare == SquareType::HORIZONTAL_PARTITION)
        {
            bool moved = false;

            for (auto& part : partitionList)
            {
                for (auto& p : part->blockList)
                {
                    if (p.row == newRow && p.col == newCol)
                    {
                        moved = trySlidePartition(part, dir);
                        break;
                    }
                }
                if (moved) break;
            }

            if (!moved)
                continue;
        }


        {
            // lock traveler to safely read current position
            lock_guard<mutex> tlock(traveler->travelerMutex);
            TravelerSegment& head = traveler->segmentList[0];

            // lock both grid squares at the same time
            std::scoped_lock gridLock(
                gridLocks[head.row][head.col],
                gridLocks[newRow][newCol]
            );

            grid[head.row][head.col] = SquareType::FREE_SQUARE;

            head.row = newRow;
            head.col = newCol;
            head.dir = dir;

            grid[newRow][newCol] = SquareType::TRAVELER;
        }
    }

    {
        // thread finished
        lock_guard<mutex> glock(globalMutex);
        numLiveThreads--;
    }
}






#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Functions called by the front end's callback functions
//-----------------------------------------------------------------------------
#endif
//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void drawAllTravelers(void)
{
    lock_guard<mutex> lock(globalMutex);
    for (size_t k = 0; k < travelerList.size(); k++)
        drawTraveler(travelerList[k]);
}



void updateMessages(void)
{
    lock_guard<mutex> lock(globalMutex);

    unsigned int numMessages = 4;
    sprintf(message[0], "We created %d travelers", numTravelers);
    sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
    sprintf(message[2], "I like cheese and coffee");
    sprintf(message[3], "Simulation run time: %ld s", time(NULL)-launchTime);

    drawMessages(numMessages, message);
}


void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;

	switch (c)
	{
		//	'esc' to quit
		case 27:
			cleanupAndQuit();
			break;

		//	slowdown
		case ',':
			slowdownTravelers();
			ok = 1;
			break;

		//	speedup
		case '.':
			speedupTravelers();
			ok = 1;
			break;

		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupTravelers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers(void)
{
	//	increase sleep time by 20%.  No upper limit on sleep time.
	//	We can slow everything down to admistrative pace if we want.
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Main, Init, and Exit
//-----------------------------------------------------------------------------
#endif


//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.
	//	So far, I hard-code some values
	numRows = 30;
	numCols = 35;
	numTravelers = 12;
	numLiveThreads = 0;
	numTravelersDone = 0;

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv);
	
	//	Now we can do application-level initialization
	initializeApplication();

	launchTime = time(NULL);

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
		
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//
//	This is a function that you have to edit and add to.
//
//==================================================================================


void initializeApplication(void)
{
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);

	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (unsigned int i=0; i<numRows; i++)
	{
		grid[i] = new SquareType[numCols];
		for (unsigned int j=0; j< numCols; j++)
			grid[i][j] = SquareType::FREE_SQUARE;
		
	}

	// V5: allocate one mutex per grid square
	gridLocks = new std::mutex*[numRows];
	for (unsigned int r = 0; r < numRows; r++)
    gridLocks[r] = new std::mutex[numCols];


	message = new char*[MAX_NUM_MESSAGES];
	for (unsigned int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
		
	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator after ranting in class that the C random
	//	generator was junk.  Here I am not using it to produce "serious" data (as in a
	//	real simulation), only wall/partition location and some color
	srand((unsigned int) time(NULL));

	//	generate a random exit
	exitPos = getNewFreePosition();
	grid[exitPos.row][exitPos.col] = SquareType::EXIT;

	//	Generate walls and partitions
	generateWalls();
	generatePartitions();
	
	travelerColor = createTravelerColors(numTravelers);



		// create all travelers
	for (unsigned int k = 0; k < numTravelers; k++)
	{
		shared_ptr<Traveler> traveler = make_shared<Traveler>();

		traveler->index = k;
		memcpy(traveler->rgba, travelerColor[k], 4 * sizeof(float));

		GridPosition pos = getNewFreePosition();
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		traveler->segmentList.push_back(seg);

		grid[pos.row][pos.col] = SquareType::TRAVELER;
		travelerList.push_back(traveler);
	}

	// start all traveler threads
	for (unsigned int k = 0; k < numTravelers; k++)
	{
		thread t(travelerThread, travelerList[k]);
		t.detach();
	}



		

		for (unsigned int k=0; k<numTravelers; k++)
			delete []travelerColor[k];
		delete []travelerColor;
}

void cleanupAndQuit()
{
	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (unsigned int i=0; i< numRows; i++)
		delete []grid[i];
	delete []grid;
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		delete []message[k];
	delete []message;

	for (unsigned int r = 0; r < numRows; r++)
		delete [] gridLocks[r];
	delete [] gridLocks;


	exit(0);
}

//------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Generation Helper Functions
#endif
//------------------------------------------------------

GridPosition getNewFreePosition(void)
{
	GridPosition pos;

	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

Direction newDirection(Direction forbiddenDir)
{
	bool noDir = true;

	Direction dir = Direction::NUM_DIRECTIONS;
	while (noDir)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));
		noDir = (dir==forbiddenDir);
	}
	return dir;
}


TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd)
{
	TravelerSegment newSeg;
	switch (currentSeg.dir)
	{
		case Direction::NORTH:
			if (	currentSeg.row < numRows-1 &&
					grid[currentSeg.row+1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row+1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::SOUTH);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::SOUTH:
			if (	currentSeg.row > 0 &&
					grid[currentSeg.row-1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row-1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::NORTH);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::WEST:
			if (	currentSeg.col < numCols-1 &&
					grid[currentSeg.row][currentSeg.col+1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col+1;
				newSeg.dir = newDirection(Direction::EAST);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::EAST:
			if (	currentSeg.col > 0 &&
					grid[currentSeg.row][currentSeg.col-1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col-1;
				newSeg.dir = newDirection(Direction::WEST);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;
		
		default:
			canAdd = false;
	}
	
	return newSeg;
}

void generateWalls(void)
{
	const unsigned int NUM_WALLS = (numCols+numRows)/4;

	//	I decide that a wall length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_WALL_LENGTH = 3;
	const unsigned int MAX_HORIZ_WALL_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_WALL_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodWall = true;
	
	//	Generate the vertical walls
	for (unsigned int w=0; w< NUM_WALLS; w++)
	{
		goodWall = false;
		
		//	Case of a vertical wall
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_WALLS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*HSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
		// case of a horizontal wall
		else
		{
			goodWall = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_WALLS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*VSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
	}
}

void generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols+numRows)/4;

	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodPart = true;

	for (unsigned int w=0; w< NUM_PARTS; w++)
	{
		goodPart = false;
		
		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_PARTS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*HSP + HSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					shared_ptr<SlidingPartition> part = make_shared<SlidingPartition>();
					part->isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						grid[row][col] = SquareType::VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part->blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}
			}
		}
		// case of a horizontal partition
		else
		{
			goodPart = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_PARTS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*VSP + VSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					shared_ptr<SlidingPartition> part = make_shared<SlidingPartition>();
					part->isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part->blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}
			}
		}
	}
}

