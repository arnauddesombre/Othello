/*
      OOOOOOO  TTTTTTT  H     H  EEEEEEE  L         L        OOOOOOO
      0     0     T     H     H  E        L         L        O     O
      0     0     T     H     H  E        L         L        O     O
      0     0     T     HHHHHHH  EEEEE    L         L        O     O
      0     0     T     H     H  E        L         L        O     O
      0     0     T     H     H  E        L         L        O     O
      0000000     T     H     H  EEEEEEE  LLLLLLL   LLLLLLL  OOOOOOO
      
      [May 2017]
 

Compile options:
g++ -Wall -c "%f" -std=c++11

Link options (with local MinGW implementation):
g++ -Wall -O3 -o "%e" "%f" -s -std=c++11 -lwinpthread

Link options (for distribution):
g++ -Wall -O3 -o "%e" "%f" -s -std=c++11 -static-libgcc -static-libstdc++ -static -lwinpthread

*/

#include <vector>
#include <iostream>
#include <thread>
#include <future>
#include <conio.h>    // _getch()
#include <iomanip>    // setw()
#include <numeric>    // accumulate()
#include <algorithm>  // find()
#include <windows.h>  // Windows specific display

enum class piece:char {EMPTY, X, O};

int BOARD_SIZE = 8; // must be >= 4, and an even number
int NUMBER_PROCESSOR = 4; // number of processor to use for parallel threading
int NUMBER_MONTE_CARLO_PATH = 20000 / NUMBER_PROCESSOR; // AI: number of Monte Carlo path per 1 thread
bool DISPLAY_COMPUTER_SCORE = true; // display score assessed for last computer's move
piece PLAYER = piece::X; // player's piece
piece COMPUTER = piece::O; // computer's piece
bool PLAYER_START = true; // player stats the game
char MODE_PLAY = 'B'; // 'M'ouse, 'K'eyboard, 'B'oth

const char DISPLAY_X = ' '; // display at the center of the X squares
const char DISPLAY_O = ' '; // display at the center of the O squares
const char DISPLAY_E = ' '; // display at the center of the EMPTY squares
const char DISPLAY_S = (char) 219; // display at the center of the SELECTED square

struct windows_console;

const char *HELP =
"How to play Othello\n"
"----------------------\n"
"\n"
"Search for the rules of the game online...\n"
"\n"
"Keyboard mode:\n"
"The red dot indicates the current cursor position. Use the arrow keys to move "
"it, and use <Spacebar> or <Enter> to make your selection.\n"
"\n"
"Mouse mode:\n"
"Use the mouse and click on the square you wish to play.\n"
"\n"
"Both mode:\n"
"Use the keyboard or the mouse to move the red dot, click on the selected square "
"or press <Spacebar> or <Enter> to validate your selection (double click or long click "
"a square to play it).\n"
"\n"
"At his turn, the computer will analyze all his possible moves, and make his play. "
"The red dot indicates his current analysis, and will remain positioned "
"on his played square.\n"
"\n"
"Press <U> to undo the last move, <F1> to display this help screen.\n"
"\n"
"Enjoy!\n"
"arnauddesombre@yahoo.com\n"
"\n";

inline int random_range(const int min, const int max) {
	return min + rand() % (max - min + 1);
}

class othello_game {
public:
	// constructor
	othello_game() {
		srand(time(0));
		for (int i = 0; i < BOARD_SIZE; i++) {
			std::vector<piece> temp;
			for (int j = 0; j < BOARD_SIZE; j++) {
				temp.push_back(piece::EMPTY);
			}
			_othelloboard_piece.push_back(temp);
			temp.clear();
		}
		// fill middle section
		const int x = BOARD_SIZE / 2;
		const int y = BOARD_SIZE / 2;
		_othelloboard_piece[x][y] = piece::O;
		_othelloboard_piece[x-1][y-1] = piece::O;
		_othelloboard_piece[x-1][y] = piece::X;
		_othelloboard_piece[x][y-1] = piece::X;
	}
	// copy constructor
	othello_game(const othello_game& game) {
		for (int i = 0; i < BOARD_SIZE; i++) {
			std::vector<piece> temp;
			for (int j = 0; j < BOARD_SIZE; j++) {
				temp.push_back(game.get_othelloboard_piece(i, j));
			}
			_othelloboard_piece.push_back(temp);
			temp.clear();
		}
	}
	// getters
	inline piece get_othelloboard_piece(const int row, const int col) const {return _othelloboard_piece[row][col];}
	inline char get_othelloboard_piece_display(const int row, const int col) const {
		switch (get_othelloboard_piece(row, col)) {
			case piece::X:
				return DISPLAY_X;
			case piece::O:
				return DISPLAY_O;
			default:
				return DISPLAY_E;
			}
	}
	// setters
	inline void set_othelloboard_piece(const int row, const int col, const piece p) {_othelloboard_piece[row][col] = p;}
	// helper function
	inline void make_move(windows_console& console, const int row, const int col, const piece p) {
		set_othelloboard_piece(row, col, p);
		execute_move(row, col, p, true, console);
	}
	inline void make_move(const int row, const int col, const piece p) {
		set_othelloboard_piece(row, col, p);
		execute_move(row, col, p);
	}
	// helper function prototypes
	void print(windows_console& console, const int row, const int col, const bool player_turn, const double score);	
	void score_move(const int rnd, const int play_row, const int play_col, std::promise<double> *result);
	std::vector<std::tuple<int, int>> valid_moves(const piece p);
	int score_board(const piece p);
	void execute_move(const int row, const int col, const piece p, const bool display, windows_console& console);
	void execute_move(const int row, const int col, const piece p);
private:
	std::vector<std::vector<piece>> _othelloboard_piece;
	// helper function prototypes
	void draw_line(windows_console& console, const char left, const char middle, const char right) const;
	void draw_first_line(windows_console& console) const;
	void draw_middle_line(windows_console& console) const;
	void draw_last_line(windows_console& console) const;
};

//----------------------------------------------------------------------------
// Display management

struct windows_console {
public:
	HANDLE _std_output;
	HANDLE _std_input;
	INPUT_RECORD _inputRecord;
	DWORD _events;
	CONSOLE_SCREEN_BUFFER_INFO _csbi;
	// create a window of given width x height
	// set the cursor to invisible, and set the title for the window
	windows_console(const short width, const short height) {
		const SMALL_RECT rect = {0, 0, (short) (width - 1), (short) (height - 1)};
		const COORD coord = {width, height};
		_std_output = GetStdHandle(STD_OUTPUT_HANDLE);
        _std_input = GetStdHandle(STD_INPUT_HANDLE);
		GetConsoleScreenBufferInfo(_std_output, &_csbi); // save defaults
		SetConsoleWindowInfo(_std_output, TRUE, &rect); // change size of console; will not resize width further than 80
		SetConsoleScreenBufferSize(_std_output, coord); // change size of console buffer
		SetConsoleMode(_std_input, ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT);
		CONSOLE_CURSOR_INFO cursorInfo = {1, false};
		SetConsoleCursorInfo(_std_output, &cursorInfo); // set cursor invisible
		SetConsoleTitle("Othello by Arnaud"); // set console title
	}
	// destructor: restore defaults, in an attempt to
	// preserve the user's console no matter what error happens
	~windows_console() {
		SetConsoleTextAttribute(_std_output, _csbi.wAttributes);
		SetConsoleScreenBufferSize(_std_output, _csbi.dwSize);
		SetConsoleWindowInfo(_std_output, TRUE, &_csbi.srWindow);
	}
	void color(const unsigned int textcol, const unsigned int backcol) {
		// set default color
		const unsigned short attributes = (backcol << 4) | textcol;
		SetConsoleTextAttribute(_std_output, attributes);
	}
	void cursor(const COORD coord = {0, 0}) {
		// reposition cursor
		SetConsoleCursorPosition(_std_output, coord);
	}
};

windows_console console_null(0, 0);

const std::string MARGIN = "  ";

// ASCII caracters:
// http://www.theasciicode.com.ar/extended-ascii-code/box-drawings-single-vertical-line-character-ascii-code-179.html

void othello_game::draw_line(windows_console& console, const char left, const char middle, const char right) const {
	std::cout << MARGIN << (char) left;
	for(int i = 0; i < BOARD_SIZE - 1; i++) {
		std::cout << std::string(7, (char) 196) << (char) middle;
	}
	std::cout << std::string(7, (char) 196) << (char) right << std::endl;
}

void othello_game::draw_first_line(windows_console& console) const {
	draw_line(console, (char) 218, (char) 194, (char) 191);
}

void othello_game::draw_middle_line(windows_console& console) const {
	draw_line(console, (char) 195, (char) 197, (char) 180);
}

void othello_game::draw_last_line(windows_console& console) const {
	draw_line(console, (char) 192, (char) 193, (char) 217);
}

// colors see: http://www.cplusplus.com/articles/2ywTURfi/
#define color_black 0
#define color_dark_blue 1
#define color_dark_green 2
#define color_dark_aqua 3
#define color_dark_cyan 3
#define color_dark_red 4
#define color_dark_purple 5
#define color_dark_pink 5
#define color_dark_magenta 5
#define color_dark_yellow 6
#define color_dark_white 7
#define color_gray 8
#define color_blue 9
#define color_green 10
#define color_aqua 11
#define color_cyan 11
#define color_red 12
#define color_purple 13
#define color_pink 13
#define color_magenta 13
#define color_yellow 14
#define color_white 15

// foreground colors
const unsigned int COL_GRID = color_black;
const unsigned int COL_TEXT = color_black;
const unsigned int COL_SEL = color_red;
const unsigned int COL_X = color_white;
const unsigned int COL_O = color_black;
// background colors
const unsigned int COL_BACK = color_dark_green;
const unsigned int COL_X_BACK = color_black;
const unsigned int COL_O_BACK = color_white;

void othello_game::print(windows_console& console, const int row, const int col, const bool player_turn, const double score) {
	// print the board on the screen
	console.cursor({0,0});
	console.color(COL_GRID, COL_BACK);
	std::cout << std::endl;
	draw_first_line(console);
	for(int i = 0; i < BOARD_SIZE; i++) {
		for(int k = 0; k < 3; k++) {
			std::cout << MARGIN <<(char) 179;
			for(int j = 0; j < BOARD_SIZE; j++) {
				if (i == row and j == col) {
					if (get_othelloboard_piece(i, j) == piece::X) {
						console.color(COL_SEL, COL_X_BACK);
					} else if (get_othelloboard_piece(i, j) == piece::O) {
						console.color(COL_SEL, COL_O_BACK);
					} else {
						console.color(COL_SEL, COL_BACK);
					}
				} else {
					if (get_othelloboard_piece(i, j) == piece::X) {
						console.color(COL_X, COL_X_BACK);
					} else if (get_othelloboard_piece(i, j) == piece::O) {
						console.color(COL_O, COL_O_BACK);
					} else {
						console.color(COL_GRID, COL_BACK);
					}
				}
				std::cout << "   ";
				if (k == 1) {
					if (i == row and j == col) {
						if (MODE_PLAY == 'K' or MODE_PLAY == 'B') {
							std::cout << DISPLAY_S;
						} else {
							std::cout << ' ';
						}
					} else {
						std::cout << ((k == 1) ? get_othelloboard_piece_display(i, j) : ' ');
					}
				} else {
					std::cout << DISPLAY_E;
				}
				std::cout << "   ";
				console.color(COL_GRID, COL_BACK);
				std::cout << (char) 179;
			}
			std::cout << std::endl;
		}
		if (i == BOARD_SIZE - 1) {
			draw_last_line(console);
		} else {
			draw_middle_line(console);
		}
	}
	console.color(COL_TEXT, COL_BACK);
	std::cout << MARGIN << "Player";
	const int size_comment = BOARD_SIZE * 8 - 13;
	if (player_turn) {
		if (size_comment > 8) {
			std::cout << std::string(size_comment / 2 -  3, ' ');
			std::cout << " [u]ndo ";
			std::cout << std::string(size_comment - size_comment / 2 - 5, ' ');
		} else {
			std::cout << std::string(size_comment, ' ');
		}
	} else {
		std::cout << std::string(size_comment, ' ');
	}
	std::cout << "Computer" << std::endl;
	std::cout << MARGIN;
	if (PLAYER == piece::X) {
		console.color(COL_X, COL_X_BACK);
		std::cout << ' ' << DISPLAY_X << ' ';
	} else {
		console.color(COL_O, COL_O_BACK);
		std::cout << ' ' << DISPLAY_O << ' ';
	}
	console.color(COL_TEXT, COL_BACK);
	std::cout << std::setfill(' ') << std::setw(3) << score_board(PLAYER);
	std::cout << std::string(BOARD_SIZE * 8 - 13, ' ');
	if (COMPUTER == piece::X) {
		console.color(COL_X, COL_X_BACK);
		std::cout << ' ' << DISPLAY_X << ' ';
	} else {
		console.color(COL_O, COL_O_BACK);
		std::cout << ' ' << DISPLAY_O << ' ';
	}
	console.color(COL_TEXT, COL_BACK);
	std::cout << std::setfill(' ') << std::setw(5) << score_board(COMPUTER) << std::endl;
	if (player_turn) {
		std::cout << MARGIN << "<play>" << std::string(BOARD_SIZE * 8 - 13, ' ');
		if (score >= 0. and DISPLAY_COMPUTER_SCORE) {
			std::cout << "   " << std::setw(4) << std::right << (int) (1000. * score) / 10. << "%" << std::endl;
		} else {
			std::cout << "        " << std::endl;
		}
	} else {
		std::cout << MARGIN << "      " << std::string(BOARD_SIZE * 8 - 13, ' ') << "  <play>" << std::endl;
	}
}

//----------------------------------------------------------------------------

const std::vector<std::tuple<int, int>> deltas = {{0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}};

std::vector<std::tuple<int, int>> othello_game::valid_moves(const piece p) {
	// return the vector of all valid moves for p
	std::vector<std::tuple<int, int>> moves;
	for (int i = 0; i < BOARD_SIZE; i++) {
		for (int j = 0; j < BOARD_SIZE; j++) {
			// check if (i,j) is a valid move
			bool valid = false;
			if (get_othelloboard_piece(i, j) == piece::EMPTY) {
				for (std::tuple<int, int> delta : deltas) {
					const int di = std::get<0>(delta);
					const int dj = std::get<1>(delta);
					for (int k = 1; k < BOARD_SIZE; k++) {
						const int new_i = i + k * di;
						const int new_j = j + k * dj;
						if (new_i >= 0 and new_i < BOARD_SIZE and new_j >= 0 and new_j < BOARD_SIZE) {
							if (k == 1) {
								if (get_othelloboard_piece(new_i, new_j) == piece::EMPTY or get_othelloboard_piece(new_i, new_j) == p) {
									break;
								}
							} else if (get_othelloboard_piece(new_i, new_j) == piece::EMPTY) {
								break;
							} else if (get_othelloboard_piece(new_i, new_j) == p) {
								valid = true;
								break;
							}
						} else {
							break;
						}
					}
					if (valid) {break;}
				}
			}
			if (valid) {
				moves.push_back({i, j});
			}
		}
	}
	return moves;
}

void othello_game::execute_move(const int row, const int col, const piece p, const bool display, windows_console& console) {
	// execute move (row,col) for p, with display of the effect
	// it is assumed to be a valid move
	for (std::tuple<int, int> delta : deltas) {
		const int drow = std::get<0>(delta);
		const int dcol = std::get<1>(delta);
		for (int k = 1; k < BOARD_SIZE; k++) {
			const int new_row = row + k * drow;
			const int new_col = col + k * dcol;
			if (new_row >= 0 and new_row < BOARD_SIZE and new_col >= 0 and new_col < BOARD_SIZE) {
				if (k == 1) {
					if (get_othelloboard_piece(new_row, new_col) == piece::EMPTY or get_othelloboard_piece(new_row, new_col) == p) {
						break;
					}
				} else if (get_othelloboard_piece(new_row, new_col) == piece::EMPTY) {
					break;
				} else if (get_othelloboard_piece(new_row, new_col) == p) {
					for (int k1 = 1; k1 < k; k1++) {
						const int new_row = row + k1 * drow;
						const int new_col = col + k1 * dcol;
						set_othelloboard_piece(new_row, new_col, p);
						if (display) {
							Sleep(100);
							print(console, -1, -1, true, 0);
						}
					}
					break;
				}
			} else {
				break;
			}
		}
	}
}

void othello_game::execute_move(const int row, const int col, const piece p) {
	// execute move (row,col) for p, without display of the effect
	// it is assumed to be a valid move
	execute_move(row, col, p, false, console_null);
}

int othello_game::score_board(const piece p) {
	// return the number of pieces p on the board
	int total = 0;
	for (int i = 0; i < BOARD_SIZE; i++) {
		for (int j = 0; j < BOARD_SIZE; j++) {
			if (get_othelloboard_piece(i, j) == p) {
				total++;
			}
		}
	}
	return total;
}

inline int computer_win(const int score_player, const int score_computer) {
	// return 1 if computer wins; -1 if player wins; 0 if tie
	if (score_player < score_computer) {
		return 1;
	} else if (score_player > score_computer) {
		return -1;
	} else {
		return 0;
	}
}

void othello_game::score_move(const int rand_seed, const int play_row, const int play_col, std::promise<double> *result) {
	// return the score assessed
	// computer has just played (play_row, play_col), so it is player's turn
	srand(rand_seed);
	int count_win = 0;
	othello_game *othello_copy1 = new othello_game(*this);
	othello_copy1->make_move(play_row, play_col, COMPUTER);
	for (int i = 0; i < NUMBER_MONTE_CARLO_PATH; i++) {
		othello_game *othello_copy2 = new othello_game(*othello_copy1);
		int turn = 1;
		int row;
		int col;
		bool game_blocked = false;
		while (true) {
			piece p;
			if (turn % 2 == 1) {
				// player's turn
				p = PLAYER;
			} else {
				// computer's turn
				p = COMPUTER;
			}
			std::vector<std::tuple<int, int>> possible_play = othello_copy2->valid_moves(p);
			if (possible_play.size() == 0) {
				// no possible move; pass
				if (game_blocked) {
					// no possible move twice; exit
					break;
				}
				game_blocked = true;
			} else {
				// play one possible move randomly
				const int rand_choice = random_range(0, possible_play.size() - 1);
				row = std::get<0>(possible_play[rand_choice]);
				col = std::get<1>(possible_play[rand_choice]);
				othello_copy2->make_move(row, col, p);
				game_blocked = false;
			}
			turn++;
		}
		const int score_player = othello_copy2->score_board(PLAYER);
		const int score_computer = othello_copy2->score_board(COMPUTER);
		if (computer_win(score_player, score_computer) == 1) {
			count_win++;
		}
		delete othello_copy2;
	}
	delete othello_copy1;
	result->set_value(1. * count_win / NUMBER_MONTE_CARLO_PATH);
}

double assess_move(othello_game& othello, windows_console& console, const int play_row, const int play_col) {
	// definition of promise/future variables
	std::promise<double> res_before[NUMBER_PROCESSOR];
	std::future<double> res_after[NUMBER_PROCESSOR];
	for (int i = 0; i < NUMBER_PROCESSOR; i++) {
		res_after[i] = res_before[i].get_future();
	}
	std::thread monte_carlo_thread[NUMBER_PROCESSOR];
	// definition of threads
	for (int i = 0; i < NUMBER_PROCESSOR; i++) {
		// it is critical to have different seeds for each thread
		const int rand_seed = (1. + i / 10.) * time(0);
		monte_carlo_thread[i] = std::thread(&othello_game::score_move, othello, rand_seed, play_row, play_col, &res_before[i]);
	}
	for (int i = 0; i < NUMBER_PROCESSOR; i++) {
		monte_carlo_thread[i].join();
	}
	std::vector<double> out;
	for (int i = 0; i < NUMBER_PROCESSOR; i++) {
		out.push_back(res_after[i].get());
	}
	// averaging of results
	const double average = std::accumulate(out.begin(), out.end(), 0.) / NUMBER_PROCESSOR;
	return average;
}

double play_player_turn(windows_console& console, othello_game &othello, int &play_row, int &play_col, const double score, std::vector<othello_game> &othello_history) {
	// Player's turn
	std::vector<std::tuple<int, int>> player_moves = othello.valid_moves(PLAYER);
	bool play_possible = (player_moves.size() > 0);
	bool first_undo = true;
	if (not play_possible) {return true;}
	SetConsoleMode(console._std_input, ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT);
	while (true) {
		othello.print(console, play_row, play_col, true, score);
		std::tuple<int, int> move = {play_row, play_col};
		bool move_done = false;
		bool undo = false;
		// necessary, though it was already done in windows_console()
		ReadConsoleInput(console._std_input, &console._inputRecord, 1, &console._events);
		const bool shift = GetKeyState(VK_SHIFT) & 0x8000;
		const bool control = GetKeyState(VK_CONTROL) & 0x8000;
		// see https://msdn.microsoft.com/en-us/library/windows/desktop/ms683499(v=vs.85).aspx
		// and https://msdn.microsoft.com/en-us/library/windows/desktop/ms684166(v=vs.85).aspx
		switch (console._inputRecord.EventType) {
			case KEY_EVENT: // keyboard input (key down) - only capture key pressed, not key released
				if (console._inputRecord.Event.KeyEvent.bKeyDown) {
					switch (console._inputRecord.Event.KeyEvent.wVirtualKeyCode) {
						case VK_LEFT: // Left
							if (shift or control) {
								play_col = 0;
							} else if (play_col > 0) { // current position is not left of board
								play_col--;
							}
							break;
						case VK_RIGHT: // Right
							if (shift or control) {
								play_col = BOARD_SIZE - 1;
							} else if (play_col < BOARD_SIZE - 1) { // current position is not right of board
								play_col++;
							}
							break;
						case VK_UP: // Up
							if (shift or control) {
								play_row = 0;
							} else if (play_row > 0) { // current position is not top of board
								play_row--;
							}
							break;
						case VK_DOWN: // Down
							if (shift or control) {
								play_row = BOARD_SIZE - 1;
							} else if (play_row < BOARD_SIZE - 1) { // current position is not bottom of board
								play_row++;
							}
							break;
						case 0x70: // F1 (function key)
							MessageBox(NULL, HELP, "Othello by Arnaud - Help", MB_OK);
							break;
						case 0x55: // U (letter)
							undo = true;
							break;
						case VK_RETURN: // Enter
						case VK_SPACE: // Space
							if (MODE_PLAY == 'K' or MODE_PLAY == 'B') {
								if (std::find(player_moves.begin(), player_moves.end(), move) != player_moves.end()) {
									move_done = true;
								}
							}
							break;
						default: // other
							break;
					}
				}
				break;

			case MOUSE_EVENT: // mouse input
				// display is:
				// from top to bottom:
				// empty line (0), grid (1), cell 0 (2,3,4), grid (5), cell 1 (6,7,8), grid (9), cell 2 (10,11,12), ...
				// from left to right:
				// MARGIN, grid, cell 0 is: 3 spaces + 'O' or 'X' + 3 spaces, grid, ...
				if (MODE_PLAY == 'M' or MODE_PLAY == 'B') {
					if(console._inputRecord.Event.MouseEvent.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
						// see https://msdn.microsoft.com/en-us/library/windows/desktop/ms684239(v=vs.85).aspx
						const int X = console._inputRecord.Event.MouseEvent.dwMousePosition.X;
						const int Y = console._inputRecord.Event.MouseEvent.dwMousePosition.Y;
						const int col = (int) ((X - MARGIN.length()) / 8);
						const int posx = (X - MARGIN.length()) % 8; // note: will be <0 for the left MARGIN area
						const int row = (int) ((Y - 1) / 4);
						const int posy = (Y - 1) % 4; // note: will be <0 for the top row
						if (posx > 0 and col >= 0 and col < BOARD_SIZE and posy > 0 and row >= 0 and row < BOARD_SIZE) {
							if (MODE_PLAY == 'M') {
								play_row = row;
								play_col = col;
								move = {play_row, play_col};
							}
							if (row == play_row and col == play_col) {
								if (std::find(player_moves.begin(), player_moves.end(), move) != player_moves.end()) {
									move_done = true;
								}
							} else {
								play_row = row;
								play_col = col;
							}
						}
					}
				}
				break;
			
			case WINDOW_BUFFER_SIZE_EVENT: // screen buffer resizing
				break; 

			case FOCUS_EVENT:  // focus events
				break;

			case MENU_EVENT:   // menu events
				break; 

			default: 
				break; 
        }
        FlushConsoleInputBuffer(console._std_input);
		if (undo) {
			if (not othello_history.empty()) {
				if (first_undo and othello_history.size() > 1) {
					// the last element of othello_history is the current position
					// hence we need to restore the preceding one for the 1st undo
					othello_history.pop_back();
					first_undo = false;
				}
				while (true) {
					othello = othello_history.back();
					if (othello_history.size() > 1) {
						othello_history.pop_back();
					}
					player_moves = othello.valid_moves(PLAYER);
					if (player_moves.size() > 0) {
						break;
					}
				}
			}
		}
		if (move_done) {
			break;
		}
	}
	othello.make_move(console, play_row, play_col, PLAYER);
	othello.print(console, play_row, play_col, true, score);
	return false;
}

double play_computer_turn(windows_console& console, othello_game &othello, int &play_row, int &play_col, double &score) {
	// Computer's turn
	const std::vector<std::tuple<int, int>> computer_moves = othello.valid_moves(COMPUTER);
	bool play_possible = (computer_moves.size() > 0);
	if (not play_possible) {return true;}
	int best_row = -1;
	int best_col = -1;
	double best_score = -0.;
	for (std::tuple<int, int> move : computer_moves) {
		const int row = std::get<0>(move);
		const int col = std::get<1>(move);
		othello.print(console, row, col, false, score);
		const double s = assess_move(othello, console, row, col);
		if (s > best_score or best_row == -1 or best_col == -1) {
			best_score = s;
			best_row = row;
			best_col = col;
		}
	}
	play_row = best_row;
	play_col = best_col;
	score = best_score;
	othello.make_move(console, play_row, play_col, COMPUTER);
	othello.print(console, best_row, best_col, false, score);
	return false;
}

void init_global_variables(int argc, char ** argv) {
	// re-initialize global variables from command line
	if (argc >= 2) {BOARD_SIZE = (int) std::atoi(argv[1]);}
	if (BOARD_SIZE < 4) {BOARD_SIZE = 4;}
	if (BOARD_SIZE % 2 == 1) {BOARD_SIZE++;}
	if (argc >= 3) {const std::string str(argv[2]); PLAYER_START = (str != "NO");}
	if (argc >= 4) {NUMBER_PROCESSOR = (int) std::atoi(argv[3]);}
	if (NUMBER_PROCESSOR < 1) {NUMBER_PROCESSOR = 1;}
	if (argc >= 5) {NUMBER_MONTE_CARLO_PATH = (int) std::atoi(argv[4]);}
	if (NUMBER_MONTE_CARLO_PATH < 100) {NUMBER_MONTE_CARLO_PATH = 100;}
	NUMBER_MONTE_CARLO_PATH = NUMBER_MONTE_CARLO_PATH / NUMBER_PROCESSOR;
	if (argc >= 6) {const std::string str(argv[5]); DISPLAY_COMPUTER_SCORE = (str != "NO");}
	if (argc >= 7) {const std::string str(argv[6]); MODE_PLAY = (char) str[0];}
	if (MODE_PLAY != 'K' and MODE_PLAY != 'M') {MODE_PLAY = 'B';}
	bool DISPLAY_MODIFS = true;
	if (argc >= 8) {const std::string str(argv[7]); DISPLAY_MODIFS = (str != "NO");}
	if (DISPLAY_MODIFS) {
		std::cout << "Per command line, othello will use:" << std::endl << std::endl;
		std::cout << "Board size             = " << BOARD_SIZE << std::endl;
		std::cout << "Player starts          = " << ((PLAYER_START) ? "YES" : "NO")  << std::endl;
		std::cout << "Monte Carlo paths      = " << NUMBER_MONTE_CARLO_PATH << " per processor" << std::endl;
		std::cout << "Input mode (K,M,B)     = " << MODE_PLAY << std::endl;
		std::cout << "Display computer score = " << ((DISPLAY_COMPUTER_SCORE) ? "YES" : "NO") << std::endl;
		if (BOARD_SIZE >= 10) {
			std::cout << std::endl << "Use the mouse to appropriately extend the window..."  << std::endl;
		}
		std::cout << std::endl << "Press any key to continue..." << std::endl;
		_getch();
	}
}

//----------------------------------------------------------------------------

int main(int argc, char ** argv){
	// update defaults
	// argv[0] is "<path>\hex.exe"
	if (argc >= 2) {
		init_global_variables(argc, argv);
	}
	windows_console console(BOARD_SIZE * 8 + 2 * MARGIN.length() + 1, BOARD_SIZE * 4 + 8);
	while (true) {
		console.color(COL_TEXT, COL_BACK);
		system("cls");
		PLAYER = (PLAYER_START) ? piece::X : piece::O;
		COMPUTER = (PLAYER_START) ? piece::O : piece::X;
		othello_game othello;
		std::vector<othello_game> othello_history;
		int play_row = BOARD_SIZE / 2 - 1;
		int play_col = BOARD_SIZE / 2 - 1;
		double score_move = 0.;
		bool end1 = false;
		bool end2 = false;
		bool player_play = PLAYER_START;
		while (not (end1 and end2)) {
			// Player's turn
			if (player_play) {
				othello_history.push_back(othello);
				end1 = play_player_turn(console, othello, play_row, play_col, score_move, othello_history);
			} else {
				player_play = true;
			}
			// Computer's turn
			end2 = play_computer_turn(console, othello, play_row, play_col, score_move);
		}
		console.color(COL_TEXT, COL_BACK);
		const int score_player = othello.score_board(PLAYER);
		const int score_computer = othello.score_board(COMPUTER);
		const int winner = computer_win(score_player, score_computer);
		std::cout << MARGIN << "Player has " << score_player << ". Computer has " << score_computer;
		if (winner == -1) {
			std::cout << ". Player wins!" << std::endl;
		} else if (winner == 1) {
			std::cout << ". Computer wins!" << std::endl;
		} else {
			std::cout << ". Tie!" << std::endl;
		}
		std::cout << MARGIN << "Press <Enter> to play again, <Esc> to continue..." << std::endl;
		int key;
		while(true) {
			key = _getch();
			if (key == 13 or key == 27) {
				break;
			}
		}
		if (key == 27) {
			break;
		} else {
			PLAYER_START = not PLAYER_START;
		}
	}
	console.color(color_white, color_black);
	system("cls");
	return 0;
}
