#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <sstream>
#include <vector>

#include "definition.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveGen.hpp"
#include "pieceTools.hpp"
#include "position.hpp"
#include "positionTools.hpp"

using namespace std;

#ifdef WITH_DATA2BIN

// Mostly copy/paste from nodchip Stockfish repository and adapted to Minic
// Tools for handling various learning data format

namespace { // anonymous

// Class that handles bitstream
// useful when doing aspect encoding
struct BitStream{
// Set the memory to store the data in advance.
// Assume that memory is cleared to 0.
void  set_data(uint8_t* data_) { 
	data = data_; 
	reset(); 
}

// Get the pointer passed in set_data().
uint8_t* get_data() const { return data; }

// Get the cursor.
int get_cursor() const { return bit_cursor; }

// reset the cursor
void reset() { bit_cursor = 0; }

// Write 1bit to the stream.
// If b is non-zero, write out 1. If 0, write 0.
void write_one_bit(int b){
	if (b){
	   //std::cout << bit_cursor << std::endl;
	   data[bit_cursor / 8] |= 1 << (bit_cursor & 7);
	}
	++bit_cursor;
	assert(bit_cursor <= 256);
}

// Get 1 bit from the stream.
int read_one_bit(){
	int b = (data[bit_cursor / 8] >> (bit_cursor & 7)) & 1;
	++bit_cursor;
	assert(bit_cursor <= 256);
	return b;
}

// write n bits of data
// Data shall be written out from the lower order of d.
void write_n_bit(int d, int n){
	for (int i = 0; i <n; ++i) write_one_bit(d & (1 << i));
}

// read n bits of data
// Reverse conversion of write_n_bit().
int read_n_bit(int n){
	int result = 0;
	for (int i = 0; i < n; ++i) result |= read_one_bit() ? (1 << i) : 0;

	return result;
}

private:
// Next bit position to read/write.
int bit_cursor;

// data entity
mutable uint8_t* data;
};

struct HuffmanedPiece
{
int code; // how it will be coded
int bits; // How many bits do you have
};

HuffmanedPiece huffman_table[] =
{
{0b0000,1}, // NO_PIECE
{0b0001,4}, // PAWN
{0b0011,4}, // KNIGHT
{0b0101,4}, // BISHOP
{0b0111,4}, // ROOK
{0b1001,4}, // QUEEN
};

// Class for compressing/decompressing sfen
struct SfenPacker{

// Pack sfen and store in data[32].
void pack(const Position& pos){
	//std::cout << GetFEN(pos) << std::endl;

	memset(data, 0, 32 /* 256bit */);
	stream.set_data(data);

	// Side to move.
	stream.write_one_bit((int)(pos.side_to_move()));

	// 7-bit positions for leading and trailing balls
	// White king and black king, 6 bits for each.
	for(auto c: {Co_White, Co_Black}) stream.write_n_bit(pos.king[c], 6);

	// Write the pieces on the board other than the kings.
	for (Rank r = Rank_8; ; --r){
		for (File f = File_a; f <= File_h; ++f){
			Piece pc = pos.board_const(MakeSquare(f, r));
			//std::cout << (int) pc << std::endl;
			if (std::abs(pc) == P_wk) continue;
			write_board_piece_to_stream(pc);
		}
		if ( r == Rank_1) break; // because rank is unsigned !!
	}

	///@todo: Support chess960.
	stream.write_one_bit(pos.castling & C_wks);
	stream.write_one_bit(pos.castling & C_wqs);
	stream.write_one_bit(pos.castling & C_bks);
	stream.write_one_bit(pos.castling & C_bqs);

	if (pos.ep == INVALIDSQUARE) {
		stream.write_one_bit(0);
	}
	else {
		stream.write_one_bit(1);
		stream.write_n_bit(static_cast<int>(pos.ep), 6);
	}

	stream.write_n_bit(pos.fifty, 6);
	stream.write_n_bit(pos.moves, 8);
	assert(stream.get_cursor() <= 256);
}

// sfen packed by pack() (256bit = 32bytes)
// Or sfen to decode with unpack()
uint8_t *data; // uint8_t[32];

BitStream stream;

// Output the board pieces to stream.
void write_board_piece_to_stream(Piece pc){
	// piece type
	auto c = huffman_table[std::abs(pc)];
	stream.write_n_bit(c.code, c.bits);
	if (pc == P_none) return;
	stream.write_one_bit(pc>0 ? Co_White : Co_Black );
}

// Read one board piece from stream
Piece read_board_piece_from_stream(){
	Piece pr = P_none;
	int code = 0, bits = 0;
	//std::cout << "------" << std::endl;
	while (true){
		//std::cout << "bits " << bits << std::endl;
		code |= stream.read_one_bit() << bits;
		//std::cout << "code " << code << std::endl;
		++bits;

		assert(bits <= 6);

		for (pr = P_none; pr < P_wk; ++pr){
			//std::cout << "pr " << (int) pr << std::endl;
			if (huffman_table[pr].code == code && huffman_table[pr].bits == bits) goto Found;
		}
	}
	Found:;
	//std::cout << "found " << int(pr) << std::endl;
	assert(pr != P_wk);
	if ( pr == P_none ) return P_none;

	const Color c = (Color)stream.read_one_bit();
	return c == Co_White ? pr : Piece(-pr);
}

}; // SfenPacker

struct PackedSfen { 
	uint8_t data[32]; 
}; 

struct PackedSfenValue{
	// phase
	PackedSfen sfen;

	// Evaluation value returned from Learner::search()
	int16_t score;

	// PV first move
	// Used when finding the match rate with the teacher
	uint16_t move;

	// Trouble of the phase from the initial phase.
	uint16_t gamePly;

	// 1 if the player on this side ultimately wins the game. -1 if you are losing.
	// 0 if a draw is reached.
	// The draw is in the teacher position generation command gensfen,
	// Only write if LEARN_GENSFEN_DRAW_RESULT is enabled.
	int8_t game_result;

	// When exchanging the file that wrote the teacher aspect with other people
	//Because this structure size is not fixed, pad it so that it is 40 bytes in any environment.
	uint8_t padding;

	// 32 + 2 + 2 + 2 + 1 + 1 = 40bytes
};

void sfen_pack(const Position & p, PackedSfen& sfen){
	SfenPacker sp;
	sp.data = (uint8_t*)&sfen;
	sp.pack(p);
}

// in order for Minic data generation and use to stay compatible with SF one, 
// I need to handle SF move encoding in the binary format ...
// So here is some SF extracted Move usage things

namespace FromSF {

enum MoveType {
	NORMAL,
	PROMOTION = 1 << 14,
	ENPASSANT = 2 << 14,
	CASTLING  = 3 << 14
};

template<MoveType T>
constexpr MiniMove MakeMove(Square from, Square to, Piece prom = P_wn) {
	return MiniMove(T + ((prom - P_wn) << 12) + (from << 6) + to);
}

constexpr MiniMove MakeMoveStd(Square from, Square to) {
	return MiniMove((from << 6) + to);
}

constexpr Square from_sq(Move m) {
  return Square((m >> 6) & 0x3F);
}

constexpr Square to_sq(Move m) {
  return Square(m & 0x3F);
}

constexpr MoveType type_of(Move m) {
  return MoveType(m & (3 << 14));
}

constexpr Piece promotion_type(Move m) {
  return Piece(((m >> 12) & 3) + P_wn);
}

constexpr Square flip_rank(Square s) { return Square(s ^ Sq_a8); }

constexpr Square flip_file(Square s) { return Square(s ^ Sq_h1); }

} // FromSF

inline MiniMove ToSFMove(const Position & p, Square from, Square to, MType type){ 
	assert(from >= 0 && from < 64); 
	assert(to >= 0 && to < 64);
	if ( isPromotion(type)) return FromSF::MakeMove<FromSF::PROMOTION>(from,to,promShift(type)); 
	else if ( type == T_ep) return FromSF::MakeMove<FromSF::ENPASSANT>(from,p.ep);
	else if ( isCastling(type) ){
		switch (type){
			case T_wks:
			return FromSF::MakeMove<FromSF::CASTLING>(from,p.rooksInit[Co_White][CT_OO]);
			case T_wqs:
			return FromSF::MakeMove<FromSF::CASTLING>(from,p.rooksInit[Co_White][CT_OOO]);
			case T_bks:
			return FromSF::MakeMove<FromSF::CASTLING>(from,p.rooksInit[Co_Black][CT_OO]);
			case T_bqs:
			return FromSF::MakeMove<FromSF::CASTLING>(from,p.rooksInit[Co_Black][CT_OOO]);
			default:
			return INVALIDMINIMOVE;
		}
	}
	else return FromSF::MakeMoveStd(from,to);		
}

inline MiniMove FromSFMove(const Position & p, MiniMove sfmove){ 
	const Square from = FromSF::from_sq(sfmove);
	const Square to = FromSF::to_sq(sfmove);
	const FromSF::MoveType SFtype = FromSF::type_of(sfmove);
	MType type = T_std;
	switch(SFtype){
		case FromSF::NORMAL:
		if (p.board_const(to) != P_none) type = T_capture;
		break;
		case FromSF::PROMOTION:
		{
			const Piece pp = FromSF::promotion_type(sfmove);
			if ( pp == P_wq ){
				type = p.board_const(to) == P_none ? T_promq : T_cappromq;
			}
			else if ( pp == P_wr ){
				type = p.board_const(to) == P_none ? T_promr : T_cappromr;
			}
			else if ( pp == P_wb ){
				type = p.board_const(to) == P_none ? T_promb : T_cappromb;
			}
			else if ( pp == P_wn ){
				type = p.board_const(to) == P_none ? T_promn : T_cappromn;
			}
		}
		break;
		case FromSF::ENPASSANT:
		type = T_ep;
		break;
		case FromSF::CASTLING:
		if ( to == p.rooksInit[Co_White][CT_OO]  ) type = T_wks;
		if ( to == p.rooksInit[Co_White][CT_OOO] ) type = T_wqs;
		if ( to == p.rooksInit[Co_Black][CT_OO]  ) type = T_bks;
		if ( to == p.rooksInit[Co_Black][CT_OOO] ) type = T_bqs;
		break;
		default:
		   assert(false);
	}
	return ToMove(from,to,type);
}

int parse_game_result_from_pgn_extract(std::string result) {
	// White Win
	if (result == "\"1-0\"") {
		return 1;
	}
	// Black Win&
	else if (result == "\"0-1\"") {
		return -1;
	}
	// Draw
	else {
		return 0;
	}
}

// 0.25 -->  0.25 * PawnValueEg
// #-4  --> -mate_in(4)
// #3   -->  mate_in(3)
// -M4  --> -mate_in(4)
// +M3  -->  mate_in(3)
ScoreType parse_score_from_pgn_extract(std::string eval, bool& success) {
	success = true;

	if (eval.substr(0, 1) == "#") {
		if (eval.substr(1, 1) == "-") {
			return -MATE + (stoi(eval.substr(2, eval.length() - 2)));
		}
		else {
			return MATE - (stoi(eval.substr(1, eval.length() - 1)));
		}
	}
	else if (eval.substr(0, 2) == "-M") {
		//std::cout << "eval=" << eval << std::endl;
		return -MATE + (stoi(eval.substr(2, eval.length() - 2)));
	}
	else if (eval.substr(0, 2) == "+M") {
		//std::cout << "eval=" << eval << std::endl;
		return MATE - (stoi(eval.substr(2, eval.length() - 2)));
	}
	else {
		char *endptr;
		double value = strtod(eval.c_str(), &endptr);
		if (*endptr != '\0') {
			success = false;
			return 0;
		}
		else {
			return ScoreType(value * 100);
		}
	}
}

inline void ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
}

inline void rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

inline void trim(std::string &s) {
	ltrim(s);
	rtrim(s);
}

// If there is a problem with the passed phase and there is an error, non-zero is returned.
int set_from_packed_sfen(Position &p, PackedSfen& sfen , bool mirror){

    //std::cout << "+++++++++++++++++++++++++" << std::endl;

	SfenPacker packer;
	auto& stream = packer.stream;
	stream.set_data((uint8_t*)&sfen);

	// Active color
	p.c = (Color)stream.read_one_bit();

	//std::cout << "color " << int(p.c) << std::endl;

#ifdef WITH_NNUE
	// clear evalList. It is cleared when memset is cleared to zero above...
	p._evalList.clear();

	// In updating the PieceList, we have to set which piece is where,
	// A counter of how much each piece has been used
	PieceId next_piece_number = PieceId::PIECE_ID_ZERO;
#endif

	// First the position of the ball
	if (mirror){
		for (auto c : {Co_White, Co_Black}){
			const Square sq = FromSF::flip_file((Square)stream.read_n_bit(6));
			p.board(sq) = c == Co_White ? P_wk : P_bk;
			p.king[c] = sq;
		}
	}
	else{
		for (auto c : {Co_White, Co_Black}){
			const Square sq = (Square)stream.read_n_bit(6);
			p.board(sq) = (c == Co_White ? P_wk : P_bk);
			p.king[c] = sq;
		}
	}

	p.kingInit[Co_White] = p.king[Co_White];
	p.kingInit[Co_Black] = p.king[Co_Black];

    //std::cout << "wk " << int(p.king[Co_White]) << std::endl;
	//std::cout << "bk " << int(p.king[Co_Black]) << std::endl;

	// Piece placement
	for (Rank r = Rank_8; ; --r){
		for (File f = File_a; f <= File_h; ++f){
			auto sq = MakeSquare(f, r);
			//std::cout << int(r) << " " << int(f) << " " << int(sq) << std::endl;
			if (mirror) {
				sq = FromSF::flip_file(sq);
			}
			Piece pc = P_none;

			// skip already given kings
			if (PieceTools::getPieceType(p,sq) != P_wk){
				assert(p.board_const(sq) == P_none);
				//std::cout << "read piece : " << int(f) << " " << int(r) << " " << int(pc) << std::endl;
				pc = packer.read_board_piece_from_stream();
			}
			else{
				//std::cout << "skipping king " << int(f) << " " << int(r) << std::endl;
			}

			// There may be no pieces, so skip in that case (also cover king case).
			if (pc == P_none) continue;

			p.board(sq) = pc;

		#ifdef WITH_NNUE
			// update evalList
			PieceId piece_no =
				(pc == P_bk) ? PieceId::PIECE_ID_BKING :// Move ball
				(pc == P_wk) ? PieceId::PIECE_ID_WKING :// Backing ball
				next_piece_number++; // otherwise

			p._evalList.put_piece(piece_no, sq, pc); // Place the pc piece in the sq box
		#endif
			//cout << sq << ' ' << board[sq] << ' ' << stream.get_cursor() << endl;

			if (stream.get_cursor()> 256) return 1;
		}
		if ( r == Rank_1) break; // because rank is unsigned !!
	}

	// Castling availability.
	p.castling = C_none;
	if (stream.read_one_bit()) {
		Square rsq;
		for (rsq = relative_square(WHITE, Sq_h1); p.board_const(rsq) != P_wr; --rsq) {}
		p.rooksInit[Co_White][CT_OO] = rsq;
		p.castling |= C_wks;
	}
	if (stream.read_one_bit()) {
		Square rsq;
		for (rsq = relative_square(WHITE, Sq_a1); p.board_const(rsq) != P_wr; ++rsq) {}
		p.rooksInit[Co_White][CT_OOO] = rsq;
		p.castling |= C_wqs;
	}
	if (stream.read_one_bit()) {
		Square rsq;
		for (rsq = relative_square(BLACK, Sq_h1); p.board_const(rsq) != P_br; --rsq) {}
		p.rooksInit[Co_Black][CT_OO] = rsq;
		p.castling |= C_bks;
	}
	if (stream.read_one_bit()) {
		Square rsq;
		for (rsq = relative_square(BLACK, Sq_a1); p.board_const(rsq) != P_br; ++rsq) {}
		p.rooksInit[Co_Black][CT_OOO] = rsq;
		p.castling |= C_bqs;
	}

	// En passant square. 
	if (stream.read_one_bit()) {
		Square ep_square = static_cast<Square>(stream.read_n_bit(6));
		if (mirror) ep_square = FromSF::flip_file(ep_square);
		p.ep = ep_square;
		///@todo ??
		/*
		if (!(attackers_to(p.ep) & pieces(sideToMove, PAWN))
		|| !(pieces(~sideToMove, PAWN) & (p.ep + pawn_push(~sideToMove)))) p.ep = INVALIDSQUARE;
		*/
	}
	else p.ep = INVALIDSQUARE;

	// Halfmove clock
	p.fifty = static_cast<Square>(stream.read_n_bit(6));

	// Fullmove number
	p.moves = static_cast<Square>(stream.read_n_bit(8));
	if (p.moves < 1) { // fix a LittleBlitzer bug here ...
		Logging::LogIt(Logging::logInfo) << "Wrong move counter " << (int)p.moves << " using 1 instead";
		p.moves = 1;
	}
	assert(stream.get_cursor() <= 256);

	p.halfmoves = (int(p.moves) - 1) * 2 + 1 + (p.c == Co_Black ? 1 : 0);

	initCaslingPermHashTable(p);

	BBTools::setBitBoards(p);
	MaterialHash::initMaterial(p);
	p.h = computeHash(p);
	p.ph = computePHash(p);

	return 0;
}

} // anonymous

bool convert_bin(const vector<string>& filenames, const string& output_file_name, 
				const int ply_minimum, const int ply_maximum, const int interpolate_eval){
	std::fstream fs;
	uint64_t data_size=0;
	uint64_t filtered_size = 0;
	uint64_t filtered_size_fen = 0;
	uint64_t filtered_size_move = 0;
	uint64_t filtered_size_ply = 0;
	// convert plain rag to packed sfenvalue for Yaneura king
	fs.open(output_file_name, ios::app | ios::binary);
	for (auto filename : filenames) {
		std::cout << "converting " << filename << " from plain to binary format... " << std::endl;
		std::string line;
		ifstream ifs;
		ifs.open(filename);
		PackedSfenValue p;
		Position pos;
		data_size = 0;
		filtered_size = 0;
		filtered_size_fen = 0;
		filtered_size_move = 0;
		filtered_size_ply = 0;
		p.gamePly = 1; // Not included in apery format. Should be initialized
		bool ignore_flag_fen = false;
		bool ignore_flag_move = false;
		bool ignore_flag_ply = false;
		while (std::getline(ifs, line)) {
			std::stringstream ss(line);
			std::string token;
			std::string value;
			ss >> token;
			if (token == "fen") {
				std::string input_fen = line.substr(4);
				readFEN(input_fen,pos,true,true);
				sfen_pack(pos,p.sfen);
			}
			else if (token == "move") {
				ss >> value;
				Square from = INVALIDSQUARE;
				Square to = INVALIDSQUARE;
				MType type = T_std;
				bool b = readMove(pos,value,from,to,type);
				if (b) {
					p.move = ToSFMove(pos,from,to,type); // use SF style move encoding
					//p.move = ToMove(from,to,type); // use Minic style move encoding
				}
			}
			else if (token == "score") {
				int16_t score;
				ss >> score;
				p.score = std::min(std::max(score,int16_t(-MATE)),int16_t(MATE));
			}
			else if (token == "ply") {
				int temp;
				ss >> temp;
				if(temp < ply_minimum || temp > ply_maximum){
					ignore_flag_ply = true;
					filtered_size_ply++;
				}
				p.gamePly = uint16_t(temp); // No cast here?
				if (interpolate_eval != 0){
				p.score = min(3000, interpolate_eval * temp);
				}
			}
			else if (token == "result") {
				int temp;
				ss >> temp;
				p.game_result = int8_t(temp); // Do you need a cast here?
				if (interpolate_eval){
				p.score = p.score * p.game_result;
				}
			}
			else if (token == "e") {
				if(!(ignore_flag_fen || ignore_flag_move || ignore_flag_ply)){
				fs.write((char*)&p, sizeof(PackedSfenValue));
				data_size+=1;
			}
				else {
					filtered_size++;
				}
				ignore_flag_fen = false;
				ignore_flag_move = false;
				ignore_flag_ply = false;
			}
		}
		std::cout << "done " << data_size << " parsed " << filtered_size << " is filtered"
				<< " (illegal fen:" << filtered_size_fen << ", illegal move:" << filtered_size_move << ", illegal ply:" << filtered_size_ply << ")" << std::endl;		ifs.close();
	}
	std::cout << "all done" << std::endl;
	fs.close();
	return true;
}

// pgn-extract --fencomments -Wlalg --nochecks --nomovenumbers --noresults -w500000 -N -V -o Fishtest.txt Fishtest.pgn
bool convert_bin_from_pgn_extract(const vector<string>& filenames, const string& output_file_name, const bool pgn_eval_side_to_move){
	
	std::cout << "pgn_eval_side_to_move=" << pgn_eval_side_to_move << std::endl;

	Position pos;

	std::fstream ofs;
	ofs.open(output_file_name, ios::out | ios::binary);

	int game_count = 0;
	int fen_count = 0;

	for (auto filename : filenames) {
		std::cout << " convert " << filename << std::endl;
		ifstream ifs;
		ifs.open(filename);

		int game_result = 0;

		std::string line;
		while (std::getline(ifs, line)) {

			if (line.empty()) {
				continue;
			}

			else if (line.substr(0, 1) == "[") {
				std::regex pattern_result(R"(\[Result (.+?)\])");
				std::smatch match;

				// example: [Result "1-0"]
				if (std::regex_search(line, match, pattern_result)) {
					game_result = parse_game_result_from_pgn_extract(match.str(1));
					//std::cout << "game_result=" << game_result << std::endl;

					game_count++;
					if (game_count % 10000 == 0) {
						std::cout << " game_count=" << game_count << ", fen_count=" << fen_count << std::endl;
					}
				}

				continue;
			}

			else {
				int gamePly = 0;
				bool first = true;

				PackedSfenValue psv;
				memset((char*)&psv, 0, sizeof(PackedSfenValue));

				auto itr = line.cbegin();

				while (true) {
					gamePly++;

					std::regex pattern_bracket(R"(\{(.+?)\})");

					std::regex pattern_eval1(R"(\[\%eval (.+?)\])");
					std::regex pattern_eval2(R"((.+?)\/)");

					// very slow
					//std::regex pattern_eval1(R"(\[\%eval (#?[+-]?(?:\d+\.?\d*|\.\d+))\])");
					//std::regex pattern_eval2(R"((#?[+-]?(?:\d+\.?\d*|\.\d+)\/))");

					std::regex pattern_move(R"((.+?)\{)");
					std::smatch match;

					// example: { [%eval 0.25] [%clk 0:10:00] }
					// example: { +0.71/22 1.2s }
					// example: { book }
					if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) {
						break;
					}

					itr += match.position(0) + match.length(0);
					std::string str_eval_clk = match.str(1);
					trim(str_eval_clk);
					//std::cout << "str_eval_clk="<< str_eval_clk << std::endl;

					if (str_eval_clk == "book") {
						//std::cout << "book" << std::endl;

						// example: { rnbqkbnr/pppppppp/8/8/8/4P3/PPPP1PPP/RNBQKBNR b KQkq - 0 1 }
						if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) {
							break;
						}
						itr += match.position(0) + match.length(0);
						continue;
					}

					// example: [%eval 0.25]
					// example: [%eval #-4]
					// example: [%eval #3]
					// example: +0.71/
					if (std::regex_search(str_eval_clk, match, pattern_eval1) ||
						std::regex_search(str_eval_clk, match, pattern_eval2)) {
						std::string str_eval = match.str(1);
						trim(str_eval);
						//std::cout << "str_eval=" << str_eval << std::endl;

						bool success = false;
						psv.score = std::min(std::max(parse_score_from_pgn_extract(str_eval, success),int16_t(-MATE)),int16_t(MATE));
						//std::cout << "success=" << success << ", psv.score=" << psv.score << std::endl;

						if (!success) {
							//std::cout << "str_eval=" << str_eval << std::endl;
							//std::cout << "success=" << success << ", psv.score=" << psv.score << std::endl;
							break;
						}
					}
					else {
						break;
					}

					if (first) {
						first = false;
					}
					else {
						psv.gamePly = gamePly;
						psv.game_result = game_result;

						if (pos.side_to_move() == BLACK) {
							if (!pgn_eval_side_to_move) {
								psv.score *= -1;
							}
							psv.game_result *= -1;
						}

#if 0
						std::cout << "write: "
								<< "score=" << psv.score
								<< ", move=" << psv.move
								<< ", gamePly=" << psv.gamePly
								<< ", game_result=" << (int)psv.game_result
								<< std::endl;
#endif

						ofs.write((char*)&psv, sizeof(PackedSfenValue));
						memset((char*)&psv, 0, sizeof(PackedSfenValue));

						fen_count++;
					}

					// example: { rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1 }
					if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) {
						break;
					}

					itr += match.position(0) + match.length(0);
					std::string str_fen = match.str(1);
					trim(str_fen);
					//std::cout << "str_fen=" << str_fen << std::endl;

					readFEN(str_fen,pos,true,true);
					sfen_pack(pos,psv.sfen);

					// example: d7d5 {
					if (!std::regex_search(itr, line.cend(), match, pattern_move)) {
						break;
					}

					itr += match.position(0) + match.length(0) - 1;
					std::string str_move = match.str(1);
					trim(str_move);
					//std::cout << "str_move=" << str_move << std::endl;

					Square from = INVALIDSQUARE;
					Square to = INVALIDSQUARE;
					MType type = T_std;
					bool b = readMove(pos,str_move,from,to,type);
					if (b) {
					psv.move = ToSFMove(pos,from,to,type); // use SF style move encoding
					//psv.move = ToMove(from,to,type); // use Minic style move encoding
					}

				}

				game_result = 0;
			}
		}
	}

	std::cout << "game_count=" << game_count << ", fen_count=" << fen_count << std::endl;
	std::cout << "all done" << std::endl;
	ofs.close();

	return true;
}

// in SF : learn convert_plain output_file_name [out] [in]
bool convert_plain(const vector<string>& filenames, const string& output_file_name){
	std::ofstream ofs;
	ofs.open(output_file_name, ios::app);
	for (auto filename : filenames) {
		std::cout << "convert " << filename << " ... " << std::endl;
		// Just convert packedsfenvalue to text
		std::fstream fs;
		fs.open(filename, ios::in | ios::binary);
		PackedSfenValue p;
		while (true){
			if (fs.read((char*)&p, sizeof(PackedSfenValue))) {
				Position tpos; // fully empty position !
				set_from_packed_sfen(tpos,p.sfen,false);
				// write as plain text
				ofs << "fen " << GetFEN(tpos) << std::endl;
				ofs << "move " << ToString(FromSFMove(tpos,p.move)) << std::endl;
				ofs << "score " << p.score << std::endl;
				ofs << "ply " << int(p.gamePly) << std::endl;
				ofs << "result " << int(p.game_result) << std::endl;
				ofs << "e" << std::endl;
			}
			else {
				break;
			}
		}
		fs.close();
		std::cout << "done" << std::endl;
	}
	ofs.close();
	std::cout << "all done" << std::endl;

	return true;
}

/*

LizardFish advices:
1) You want at least 100m positions. And validation should be at least 2 ply deeper than training data
2) up your eval save to the biggest you can manage. 100m? 250m? 500m? Depends on how many you have.
3) reduce decay to 0.1. Gives maybe extra 10 elo.
4) Test the final two to three nets. The last is not always the best.
5) get another 10% of really deep data (and even deeper validation data). 
   Run with your phase 1 net with eta 0.05, lambda 0.7, nn batch 10000, eval save 10m. 
   This can add another 50 elo.
6) For the “sharpening” with deeper data, go to 10000 for the nn_batch_size.

training line : 

* from scratch

uci
isready
setoption name Use NNUE value true
setoption name threads value 7
setoption name skiploadingeval value true
isready
learn targetdir train_data/random_12/ loop 100 batchsize 1000000 use_draw_in_training 1 eta 1 lambda 1 eval_limit 32000 nn_batch_size 1000 newbob_decay 0.1 eval_save_interval 50000000 loss_output_interval 10000000 mirror_percentage 50 validation_set_file_name train_data/validation/validation.plain.bin

* from an existing nn :

uci
isready
setoption name Use NNUE value true
setoption name EValFile value nn.bin
setoption name threads value 7
setoption name skiploadingeval value false
isready
learn targetdir train_data/random_12/ loop 100 batchsize 1000000 use_draw_in_training 1 eta 1 lambda 1 eval_limit 32000 nn_batch_size 1000 newbob_decay 0.1 eval_save_interval 50000000 loss_output_interval 10000000 mirror_percentage 50 validation_set_file_name train_data/validation/validation.plain.bin

*/



#endif // WITH_DATA2BIN