//
//	bgrep version 1.5
//
//	機能
//		バイナリファイル中からパターンを検索します。
//	コンパイル例
//		Visual C++
//			cl bgrep.cpp /EHsc
//		g++
//			g++ -o bgrep bgrep.cpp -std=c++0x -static -lboost_program_options -lboost_system
//	未対応
//		・2Gバイト超のファイルからは検索できません
//
#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/tokenizer.hpp>

// ユーザー定義の例外
class MyException : public std::runtime_error
{
public:
	explicit MyException(const std::string& msg) : std::runtime_error(msg) {}
	virtual ~MyException() throw() {}
};

// ユーザー定義の例外（使用法を表示して終了させたいときに使用）
class ShowUsageException : public std::runtime_error
{
public:
	ShowUsageException() : std::runtime_error("") {}
	//explicit ShowUsageException(const std::string& msg) : std::runtime_error(msg) {}
	virtual ~ShowUsageException() throw() {}
};

// stringをwstringに変換
std::wstring
widen(const std::string& input) {
	wchar_t* buffer = new wchar_t[input.size() + 1];
	size_t ret_size = 0;
	mbstowcs_s(&ret_size, buffer, input.size() + 1, input.c_str(), input.size() + 1);
	std::wstring result = buffer;
	delete[] buffer;
	return result;
}


typedef unsigned char byte_t;

// 16進文字を数値に変換する
byte_t								// 00～0F
hexchr_to_val(
	std::string::value_type chr		// '0'～'F'
) throw(MyException)
{
	if ('0' <= chr && chr <= '9') {
		return static_cast<byte_t>(chr - '0');
	}
	else if ('a' <= chr && chr <= 'f') {
		return static_cast<byte_t>(10 + chr - 'a');
	}
	else if ('A' <= chr && chr <= 'F') {
		return static_cast<byte_t>(10 + chr - 'A');
	}
	else {
		throw MyException(std::string("illegal character '") + chr + '\'');
	}
}

// バイナリファイル中からパターンを検索する
template <class PatternType, typename Function>
bool								// true：見つかった、false：見つからなかった
search_in_file(
	std::ifstream& fi,				// バイナリファイル
	const PatternType& pattern,		// 検索パターン（バイトのコンテナ）
	bool quick,						// true：1つでも見つかったら処理を終了する、false：全部検索する
	Function call_if_found			// 見つかったときに呼ばれる関数
) throw(MyException)
{
	static_assert(sizeof(typename PatternType::value_type) == 1, "invalid size - PatternType::value_type.");
	static const size_t BUFSIZE = 65536;
	static typename PatternType::value_type buf[BUFSIZE];		// ファイルからこのバッファに読み込む

	// パターンの長さがゼロなら、見つからなかったとみなす
	if (pattern.empty()) {
		return false;
	}

	// パターンが長すぎたらエラーにする
	const size_t pattern_size = pattern.size();
	if (pattern_size <= BUFSIZE / 10) {		// (BUFSIZE / 10)に特別な意味はない
		;
	}
	else {
		throw MyException("pattern too long");
	}

	typename PatternType::value_type* const read_buf_pos = buf + (pattern_size - 1);	// ファイルからこのバッファの位置に読み込む
	const int read_size = BUFSIZE - (pattern_size - 1);									// ファイルから読み込むバイト数

	bool result = false;
	size_t read_file_pos = 0;	// 読み込んだデータの、ファイル先頭からの位置（パターンが見つかったときの位置の計算に使用する）
	fi.read(reinterpret_cast<char* const>(read_buf_pos), read_size);
	const typename PatternType::value_type* first = read_buf_pos;	// バッファの検索開始位置（初回は読み込んだ位置から検索）
	while (fi.gcount()) {
		const typename PatternType::value_type* last = read_buf_pos + fi.gcount();		// バッファの検索終了位置
		while (last != (first = std::search(first, last, pattern.begin(), pattern.end()))) {
			// 見つかったときの処理
			const size_t found_fpos_zero_origin = first - read_buf_pos + read_file_pos;	// 見つかったときの、ファイル先頭からの位置
			call_if_found(found_fpos_zero_origin);

			if (quick) {
				return true;
			}
			result = true;
			++first;		// 見つかった位置の1バイト後ろから引き続き検索
		}
		read_file_pos += fi.gcount();
		std::copy(last - (pattern_size - 1), last, buf);					// バッファの末尾を先頭にコピーして．．
		fi.read(reinterpret_cast<char* const>(read_buf_pos), read_size);	// コピーした後ろの位置に読み込む
		first = buf;														// バッファの先頭から検索する
	}
	return result;
}

// メイン
int main(int argc, char* argv[])
{
	namespace po = boost::program_options;
	namespace fs = boost::filesystem;

	// ハイフン付オプションを定義
	po::options_description opt_hyphen("Options");

	try {
		setlocale(LC_CTYPE, "");

		opt_hyphen.add_options()
			("string,s", "string search <default>")
			("wstring,w", "wstring search")
			("hex,h", "hex search")
			("list,l", "list match files")
			("verbose,v", "display messages")
			;

		// ハイフンなしオプションを定義
		po::options_description opt_pattern_and_files("Command arguments");
		opt_pattern_and_files.add_options()
			("pattern_and_files", po::value<std::vector<std::string>>(), "pattern and files")
			;

		po::positional_options_description posi;
		posi.add("pattern_and_files", -1);

		// 全オプションを variables_mapに格納
		po::variables_map varmap;
		po::options_description full("All options");
		full.add(opt_hyphen).add(opt_pattern_and_files);

		po::store(po::command_line_parser(argc, argv).options(full).positional(posi).run(), varmap);
		po::notify(varmap);

		// ハイフン付オプションの取り出し
		int opt_string = varmap.count("string");
		int opt_wstring = varmap.count("wstring");
		int opt_hex = varmap.count("hex");

		switch (opt_string + opt_wstring + opt_hex) {
		case 0:
			opt_string = 1;		// デフォルトは"文字列を検索"
			break;
		case 1:
			break;
		default:
			throw MyException("options contradict");
		}

		const bool opt_list_files = (varmap.count("list") != 0);
		const bool opt_verbose = (varmap.count("verbose") != 0);

		// コマンドラインの表示
		if (opt_verbose) {
			//std::copy(argv, argv + argc, std::ostream_iterator<const char*>(std::cout, " "));
			std::for_each(argv, argv + argc, [](const char* arg)
				{
					if (strchr(arg, ' ') != NULL) {
						// スペースを含む引数は、ダブルクォーテーションで囲って表示
						std::cout << '\"' << arg << '\"';
					}
					else {
						std::cout << arg;
					}
					std::cout << ' ';
				}
			);
			std::cout << "\n\n";
		}

		// ハイフンなしオプションの取り出し
		if (varmap.count("pattern_and_files")) {
			;
		}
		else {
			throw ShowUsageException();
		}
		const std::vector<std::string>& pattern_and_files(varmap["pattern_and_files"].as<std::vector<std::string>>());
		if (2 <= pattern_and_files.size()) {
			;
		}
		else {
			throw MyException("short of parameter");
		}

		// ハイフンなしオプションの1番目は、検索パターン
		std::vector<std::string>::const_iterator itr = pattern_and_files.begin();
		const std::string& pattern_arg(*itr);

		// 以後、数値は16進表示
		std::cout << std::hex << std::setiosflags(std::ios::uppercase);

		///////// オプションに応じて検索パターンを変換し、16進表示する

		std::vector<byte_t> byte_array;		// 変換後のバイトを格納するコンテナ

		if (opt_verbose) {
			// 検索の種類（文字列、ワイド文字列、16進数）の表示
			std::cout << "search option: ";
			if (opt_string) {
				std::cout << "[string] wstring hex\n";
			}
			else if (opt_wstring) {
				std::cout << "string [wstring] hex\n";
			}
			else {
				std::cout << "string wstring [hex]\n";
			}

			// 検索パターンの表示
			std::cout << "pattern (arg): " << pattern_arg << '\n';
			std::cout << "pattern (hex): ";
		}

		if (opt_string) {			// 文字列を探索
			// 検索パターンを16進表示
			if (opt_verbose) {
				//std::copy(pattern_arg.begin(), pattern_arg.end(), std::ostream_iterator<byte_t>(std::cout, " "));
				std::for_each(pattern_arg.begin(), pattern_arg.end(), [](byte_t chr) { std::cout << static_cast<unsigned int>(chr) << ' '; });
			}

		}
		else {
			if (opt_wstring) {		// 文字列をワイド文字列に変換して検索
				// stringをwstringに変換
				std::wstring wstr(widen(pattern_arg));

				// wstringをバイトのコンテナに変換
				byte_array.reserve(wstr.size() * sizeof(std::wstring::value_type));
				std::for_each(wstr.begin(), wstr.end(), [&byte_array](const std::wstring::value_type& value)
					{
						auto pp = reinterpret_cast<const byte_t*>(&value);
						std::copy(pp, pp + sizeof(std::wstring::value_type), std::back_inserter(byte_array));
					}
				);

			}
			else {				// 16進数を探索
				// 16進文字列をバイナリデータに変換
#if 0
				std::istringstream is(pattern_arg);
				std::string hexstr;
				while (is >> hexstr) {
					byte_t hex;

					switch (hexstr.size()) {
					case 1:
						hex = hexchr_to_val(hexstr[0]);
						break;
					case 2:
					{
						const unsigned int high = hexchr_to_val(hexstr[0]);
						const unsigned int low = hexchr_to_val(hexstr[1]);
						hex = static_cast<byte_t>((high << 4) | low);
					}
					break;
					default:
						throw MyException("An illegal hex：There are 3 characters and more tokens.");
					}

					byte_array.push_back(hex);
				}
#else
				// boost::tokenizerを使って書き換えてみた
				boost::char_separator<char> sep(" \t\n");
				boost::tokenizer<boost::char_separator<char>> tokens(pattern_arg, sep);
				std::transform(tokens.begin(), tokens.end(), std::back_inserter(byte_array)
					, [](const std::string& hexstr) -> byte_t
					{
						if (hexstr.size() == 1 || hexstr.size() == 2) {
							try {
								if (hexstr.size() == 1) {
									return hexchr_to_val(hexstr[0]);
								}
								else {
									const unsigned int high = hexchr_to_val(hexstr[0]);
									const unsigned int low = hexchr_to_val(hexstr[1]);
									return static_cast<byte_t>((high << 4) | low);
								}
							}
							catch (MyException& ex) {
								throw MyException("illegal hex string: \"" + hexstr + "\" -> " + ex.what());
							}
						}
						else {
							throw MyException("illegal hex string: \"" + hexstr + "\" -> there are 3 characters and more tokens");
						}
					}
				);
#endif
			}

			// 検索パターンを16進表示
			if (opt_verbose) {
				//std::copy(byte_array.begin(), byte_array.end(), std::ostream_iterator<byte_t>(std::cout, " "));
				std::for_each(byte_array.begin(), byte_array.end(), [](byte_t chr) { std::cout << static_cast<unsigned int>(chr) << ' '; });
			}
		}

		if (opt_verbose) {
			std::cout << "\n----------(begin)----------\n";
		}

		// ハイフンなしオプションの2番目以降は、ファイル名
		for (++itr; itr != pattern_and_files.end(); ++itr) {
			const std::string& filename(*itr);

			// ファイルオープン
			//std::ifstream fi(filename.c_str(), std::ios::nocreate | std::ios::binary);
			std::ifstream fi(filename.c_str(), std::ios::binary);
			if (fi) {
				;
			}
			else {
				throw MyException("could not open \"" + filename + '"');
			}

			// パターンが見つかったとき、この関数で表示する
			auto print_if_found = [&filename, opt_list_files](size_t fpos_zero_origin)
			{
				std::cout << filename;
				if (opt_list_files) {
					// -lオプションが指定されたときは、ファイル名のみ表示
					;
				}
				else {
					// -lオプションが指定されなかったときは、ファイル名とファイル先頭からの位置を表示
					std::cout << ": " << fpos_zero_origin;
				}
				std::cout << std::endl;
			};

			if (opt_string) {
				// 与えられたパターンを、そのまま文字列として検索
				search_in_file(fi, pattern_arg, opt_list_files, print_if_found);
			}
			else {
				// 与えられたパターンから変換した、バイトのコンテナを探索
				search_in_file(fi, byte_array, opt_list_files, print_if_found);
			}
		}
		if (opt_verbose) {
			std::cout << "-----------(end)-----------" << std::endl;
		}
	}
	catch (ShowUsageException& ex) {
		if (ex.what()[0]) {
			std::cerr << "Error: " << ex.what() << '\n' << std::endl;
		}
		std::cout << "bgrep Version 1.5 Copyright (c) 2011 kanedaq\n";
		std::cout << "Usage: bgrep [-swhlv] pattern file[s]\n";
		std::cout << opt_hyphen;
		std::cout << "Usage examples:\n";
		std::cout << "    (Un*x) find . -type f -name '*' -print0 | xargs -0 bgrep pattern\n";
		std::cout << "    (Windows) dir /b /a-d /s * | xargs bgrep pattern" << std::endl;
		return 1;
	}
	catch (std::exception& ex) {
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}
