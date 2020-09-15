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

#include "learn_tools.hpp"

#ifdef WITH_DATA2BIN

// Mostly copy/paste from nodchip Stockfish repository and adapted to Minic
// Tools for handling various learning data format

namespace{
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

} // anonymous

bool convert_bin(const std::vector<std::string>& filenames, const std::string& output_file_name, 
				const int ply_minimum, const int ply_maximum, const int interpolate_eval){
	std::fstream fs;
	uint64_t data_size=0;
	uint64_t filtered_size = 0;
	uint64_t filtered_size_fen = 0;
	uint64_t filtered_size_move = 0;
	uint64_t filtered_size_ply = 0;
	// convert plain rag to packed sfenvalue for Yaneura king
	fs.open(output_file_name, std::ios::app | std::ios::binary);
	for (auto filename : filenames) {
		std::cout << "converting " << filename << " from plain to binary format... " << std::endl;
		std::string line;
		std::ifstream ifs;
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
				p.score = std::min(3000, interpolate_eval * temp);
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

// pgn-extract --fencomments -Wlalg --nochecks --nomovenumbers --noresults -w500000 -N -V -o data.plain games.pgn
bool convert_bin_from_pgn_extract(const std::vector<std::string>& filenames, const std::string& output_file_name, const bool pgn_eval_side_to_move){
	
	std::cout << "pgn_eval_side_to_move=" << pgn_eval_side_to_move << std::endl;

	Position pos;

	std::fstream ofs;
	ofs.open(output_file_name, std::ios::out | std::ios::binary);

	int game_count = 0;
	int fen_count = 0;

	for (auto filename : filenames) {
		std::cout << " convert " << filename << std::endl;
		std::ifstream ifs;
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
bool convert_plain(const std::vector<std::string>& filenames, const std::string& output_file_name){
	std::ofstream ofs;
	ofs.open(output_file_name, std::ios::app);
	for (auto filename : filenames) {
		std::cout << "convert " << filename << " ... " << std::endl;
		// Just convert packedsfenvalue to text
		std::fstream fs;
		fs.open(filename, std::ios::in | std::ios::binary);
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