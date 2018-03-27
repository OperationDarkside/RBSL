#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <array>

enum CmdType {
	RETURN,
	ASSIGN,
	METHOD_CALL,
	FUNC
};

enum PtrType {
	TEMPORARY,
	STATIC
};

class var_ptr {
public:
	PtrType type;
	unsigned short ptr = 0;
};

class CompiledCmd {
public:
	CmdType type;

	unsigned short tmp_ptr = 0;
	unsigned short static_ptr = 0;

	unsigned short func_id = 0;

	std::string return_content;

	std::vector<var_ptr> vars;
};

std::vector<char> read_file (const std::string& filename) {
	std::ifstream input;
	std::streamsize size;

	input.open (filename, std::ios::binary | std::ios::ate);
	size = input.tellg ();
	input.seekg (0, std::ios::beg);

	std::vector<char> buffer;

	if (!input.is_open ()) {
		std::cout << "file not open";
		return buffer;
	}

	if (!input.good ()) {
		std::cout << "file not good";
		return buffer;
	}

	buffer.resize (size);

	if (!input.read (buffer.data (), size)) {
		std::cout << "file didn't read";
		return buffer;
	}

	return buffer;
}

unsigned short u_char_to_u_short (std::array<unsigned char, 2> val) {
	unsigned short res = 0;

	res = val[1] << 8;
	res |= val[0];

	return res;
}

size_t read_return (CompiledCmd& cmd, std::vector<unsigned char>& byte_code, size_t pos) {
	unsigned short len = u_char_to_u_short ({byte_code[pos], byte_code[pos + 1]});

	if (len > 2) {
		cmd.tmp_ptr = u_char_to_u_short ({byte_code[pos + 2], byte_code[pos + 3]});

		std::vector<char> tmp_content (byte_code.begin () + pos + 4, byte_code.begin () + pos + 2 + len);

		cmd.return_content = std::string (tmp_content.begin (), tmp_content.end ());
	}

	return pos + 2 + len;
}

size_t read_assign (CompiledCmd& cmd, std::vector<unsigned char>& byte_code, size_t pos) {
	unsigned short len = u_char_to_u_short ({byte_code[pos], byte_code[pos + 1]});

	if (len == 4) {
		cmd.static_ptr = u_char_to_u_short ({byte_code[pos + 2], byte_code[pos + 3]});
		cmd.tmp_ptr = u_char_to_u_short ({byte_code[pos + 4], byte_code[pos + 5]});
	}

	return pos + 2 + len;
}

size_t read_func (CompiledCmd& cmd, std::vector<unsigned char>& byte_code, size_t pos) {
	unsigned short len = u_char_to_u_short ({byte_code[pos], byte_code[pos + 1]});

	if (len > 1) {
		cmd.func_id = u_char_to_u_short ({byte_code[pos + 2], byte_code[pos + 3]});
		cmd.tmp_ptr = u_char_to_u_short ({byte_code[pos + 4], byte_code[pos + 5]});

		size_t param_num = (len - 4) / 3;
		for (size_t i = 0; i < param_num; i++) {
			var_ptr var;
			var.type = static_cast<PtrType>(byte_code[pos + 6 + i * 3]);
			var.ptr = u_char_to_u_short ({byte_code[pos + 6 + i * 3 + 1], byte_code[pos + 6 + i * 3 + 2]});
			cmd.vars.push_back (var);
		}
	}

	return pos + 2 + len;
}

std::vector<CompiledCmd> bytecode_to_cmds (std::vector<unsigned char>& byte_code) {
	std::vector<CompiledCmd> res;

	// 1 Byte : Instruction Type
	// 2 Bytes: Body Length
	// BODY
	// RETURN: 2 Bytes Create Temporary Variable Pointer
	// ASSIGN: 2 Bytes Target Static Variable Pointer <-- 2 Bytes Source Temporary Variable Pointer
	// FUNC  : 2 Bytes Function Type + 2 Bytes Temporary Return Value Pointer + ParameterCount * (1 Byte Pointer Type + 2 Bytes Pointer)

	size_t len = byte_code.size ();

	for (size_t i = 0; i < len; ++i) {
		size_t new_pos = 0;
		CompiledCmd cmd;

		CmdType type = static_cast<CmdType>(byte_code[i]);
		cmd.type = type;
		switch (type) {
		case CmdType::RETURN:
			new_pos = read_return (cmd, byte_code, i + 1);
			i = new_pos - 1; // because i++
			res.push_back (cmd);
			break;
		case CmdType::ASSIGN:
			new_pos = read_assign (cmd, byte_code, i + 1);
			i = new_pos - 1; // because i++
			res.push_back (cmd);
			break;
		case CmdType::FUNC:
			new_pos = read_func (cmd, byte_code, i + 1);
			i = new_pos - 1; // because i++
			res.push_back (cmd);
			break;
			/*default:
				throw "Unknown command";
				break;*/
		}
	}

	return res;
}

int main () {

	std::vector<char> file_content = read_file ("new_script.bc");

	if (file_content.size () == 0) {
		return 1;
	}

	std::vector<unsigned char> byte_code (file_content.begin (), file_content.end ());

	std::vector<CompiledCmd> cmds = bytecode_to_cmds (byte_code);

	return 0;
}