#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <array>

class Variable {
public:
	std::string name;
	std::string value;
};

enum CmdType {
	RETURN,
	ASSIGN,
	METHOD_CALL,
	FUNC
};

class Cmd {
public:
	CmdType type;

	std::string func_name;
	std::vector<std::string> vars;
	std::vector<Cmd> sub_cmd;
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

std::unordered_map<std::string, unsigned short> function_map;

std::string read_file (const std::string& filename) {
	std::string content;
	std::ifstream input;
	std::streamsize size;

	input.open (filename, std::ios::binary | std::ios::ate);
	size = input.tellg ();
	input.seekg (0, std::ios::beg);

	std::vector<char> buffer (size);

	if (!input.is_open ()) {
		std::cout << "file not open";
		return content;
	}

	if (!input.good ()) {
		std::cout << "file not good";
		return content;
	}

	if (!input.read (buffer.data (), size)) {
		std::cout << "file didn't read";
		return content;
	}

	content.insert (content.begin (), buffer.begin (), buffer.end ());

	return content;
}

void write_file (const std::string& filename, const std::vector<unsigned char>& data) {
	std::ofstream output;

	output.open (filename, std::ios::binary);

	if (!output.is_open()) {
		std::cout << "Output file not open";
		return;
	}

	const char* signed_data = reinterpret_cast<const char*>(data.data ());

	output.write(signed_data, data.size());

	output.close ();
}

std::string clean_content (const std::string& content) {
	bool is_string = false;
	size_t len = content.size ();
	std::string new_content;

	new_content.reserve (content.size ());

	for (size_t i = 0; i < len; ++i) {
		char c = content[i];
		if (c == '\"') {
			if (content[i - 1] != '\\' && !is_string) {
				is_string = true;
			} else {
				is_string = false;
			}
		}
		if ((c == ' ' || c == '\r' || c == '\n') && !is_string) {
			continue;
		}
		new_content += c;
	}

	return new_content;
}

std::vector<std::string> separate (const std::string& content) {
	bool is_string = false;
	size_t len = content.size ();
	std::string section;
	std::vector<std::string> sections;


	for (size_t i = 0; i < len; ++i) {
		char c = content[i];

		if (c == '\"') {
			if (content[i - 1] != '\\' && !is_string) {
				is_string = true;
			} else {
				is_string = false;
			}
		}

		if (c == ';' && !is_string) {
			sections.push_back (section);
			section.clear ();
			continue;
		}
		section += c;
	}

	return sections;
}

Cmd read_var (const std::string& content, size_t start_index, size_t len) {
	Cmd cmd;
	std::string var;

	for (size_t i = start_index; i < len; ++i) {
		char c = content[i];

		if (c == '=') {
			cmd.vars.push_back (var);
			cmd.type = CmdType::ASSIGN;
			break;
		} else if (c == '.') {
			cmd.vars.push_back (var);
			cmd.type = CmdType::METHOD_CALL;
			break;
		} else {
			var += c;
		}
	}

	return cmd;
}

std::string read_string (const std::string& content, size_t start_index, size_t len) {
	return content.substr (start_index + 1, len - start_index - 2);
}

Cmd read_func (const std::string& content, size_t start_index, size_t len) {
	bool is_func_name = true;
	bool is_var = false;
	std::string func_name;
	std::string var;
	std::vector<std::string> variables;
	Cmd cmd;

	for (size_t i = start_index; i < len; ++i) {
		char c = content[i];

		if (c == '(') {
			is_func_name = false;
		} else if (c == ')') {
			if (!var.empty ()) {
				variables.push_back (var);
			}
			break;
		}

		if (is_func_name) {
			func_name += c;
			continue;
		}

		if (c == ',') {
			variables.push_back (var);
			var.clear ();
			is_var = false;
			continue;
		}

		if (c == '#') {
			is_var = true;
		}

		if (is_var) {
			var += c;
		}
	}

	cmd.type = CmdType::FUNC;
	cmd.func_name = func_name;
	cmd.vars = variables;

	return cmd;
}

Cmd analyse_subsection (const std::string& content, size_t start_index, size_t len) {
	bool is_string = false;
	bool is_function = false;
	bool is_other = false;
	Cmd cmd;
	std::string var;

	switch (content[start_index]) {
	case '\"': // string
		is_string = true;
		break;
	case '$': // function
		is_function = true;
		break;
	default: // some other value
		is_other = true;
		break;
	}

	if (is_string) {
		cmd.type = CmdType::RETURN;
		cmd.vars.push_back (read_string (content, start_index, len));
		return cmd;
	}
	if (is_function) {
		return read_func (content, start_index, len);
	}

	return cmd;
}

std::vector<Cmd> analyse_section (const std::string& section) {
	bool is_string = false;
	bool is_variable = false;
	bool is_function = false;
	bool is_class = false;
	size_t len = section.size ();
	std::string var;
	std::vector<Cmd> cmds;

	switch (section[0]) {
	case '#':
		is_variable = true;
		break;
	case '$':
		is_function = true;
		break;
	case '@':
		is_class = true;
	default:
		throw "Undefined section beginning!";
		break;
	}

	if (is_variable) {
		Cmd cmd = read_var (section, 0, len);
		if (cmd.type == CmdType::ASSIGN) {
			cmd.sub_cmd.push_back (analyse_subsection (section, section.find_first_of ('=', 0) + 1, len));
		}
		cmds.push_back (cmd);
	}
	if (is_function) {
		Cmd cmd = read_func (section, 0, len);
		cmds.push_back (cmd);
	}

	return cmds;
}

/*void read_var (const std::string& line, std::vector<Variable>& vars) {
	bool is_name = true;
	Variable var;

	for (char c : line) {
		if (c == '#' || c == ' ' || c == ';') {
			continue;
		}
		if (c == '=') {
			is_name = false;
			continue;
		}

		if (is_name) {
			var.name += c;
		} else {
			var.value += c;
		}
	}

	vars.push_back (var);
}
*/

void flatten_commands (std::vector<Cmd>& sub_cmds, std::vector<Cmd>& all_sub_cmds) {
	for (auto& cmd : sub_cmds) {
		if (cmd.sub_cmd.size () > 0) {
			flatten_commands (cmd.sub_cmd, all_sub_cmds);
		}
		all_sub_cmds.push_back (cmd);
	}
}

void compile_assign (Cmd& cmd, Cmd& prev_cmd, CompiledCmd& comp_cmd, CompiledCmd& prev_comp_cmd, std::unordered_map<std::string, unsigned short>& static_ptrs, unsigned short& static_ptr_cntr) {
	if (cmd.vars.size () != 1) {
		throw "ASSIGN: Wrong variable count";
	}
	auto var_res = static_ptrs.find (cmd.vars[0]);
	if (var_res != static_ptrs.end ()) {
		// found
		comp_cmd.static_ptr = var_res->second;
	} else {
		// not found
		unsigned short new_static_ptr = ++static_ptr_cntr;
		static_ptrs.emplace (cmd.vars[0], new_static_ptr);
		comp_cmd.static_ptr = new_static_ptr;
	}

	bool is_not_return = prev_cmd.type != CmdType::RETURN;
	bool is_not_func = prev_cmd.type != CmdType::FUNC;

	if (!(is_not_return || is_not_func)) {
		throw "ASSIGN: Previous command doesn't return temporary ptr";
	}
	var_ptr ptr;
	ptr.type = PtrType::TEMPORARY;
	ptr.ptr = prev_comp_cmd.tmp_ptr;
	comp_cmd.vars.push_back (ptr);
}

void compile_func (Cmd& cmd, CompiledCmd& comp_cmd, std::unordered_map<std::string, unsigned short>& static_ptrs, unsigned short& tmp_ptr_cntr) {
	comp_cmd.tmp_ptr = ++tmp_ptr_cntr;
	comp_cmd.func_id = function_map.at (cmd.func_name);
	for (auto& var : cmd.vars) {
		auto var_value = static_ptrs.find (var);
		if (var_value == static_ptrs.end ()) {
			throw "FUNC: Variable not found" + var;
		}
		var_ptr new_var_ptr;
		new_var_ptr.type = PtrType::STATIC;
		new_var_ptr.ptr = var_value->second;
		comp_cmd.vars.push_back (new_var_ptr);
	}
}

std::vector<CompiledCmd> compile (std::vector<Cmd>& cmds) {
	unsigned short tmp_ptr_cntr = 0;
	unsigned short static_ptr_cntr = 0;
	std::unordered_map<std::string, unsigned short> static_ptrs;

	std::vector<CompiledCmd> res;

	size_t len = cmds.size ();
	for (size_t i = 0; i < len; ++i) {
		Cmd& cmd = cmds[i];
		CompiledCmd compiled_cmd;

		compiled_cmd.type = cmd.type;

		switch (cmd.type) {
		case CmdType::RETURN:
			if (cmd.vars.size () != 1) {
				throw "RETURN: No variable found";
			}
			compiled_cmd.tmp_ptr = ++tmp_ptr_cntr;
			compiled_cmd.return_content = cmd.vars[0];
			break;
		case CmdType::FUNC:
			compile_func (cmd, compiled_cmd, static_ptrs, tmp_ptr_cntr);
			break;
		case CmdType::ASSIGN:
			if (i == 0) {
				throw "COMPILE: Couldn't find previous command";
			}
			compile_assign (cmd, cmds[i - 1], compiled_cmd, res[i - 1], static_ptrs, static_ptr_cntr);
			break;
		default:
			throw "Unknown Cmd Type";
			break;
		}

		res.push_back (compiled_cmd);
	}

	return res;
}

std::array<unsigned char, sizeof (unsigned short)> u_short_to_u_char (unsigned short& val) {
	std::array<unsigned char, sizeof (unsigned short)> res;

	res[0] = val & 0xff;
	res[1] = (val >> 8) & 0xff;

	return res;
}

std::vector<unsigned char> to_byte_code (std::vector<CompiledCmd>& cmds) {
	std::vector<unsigned char> res;

	for (auto& cmd : cmds) {
		unsigned short len = 0;

		unsigned char type = static_cast<unsigned char>(cmd.type);
		res.push_back (type);

		switch (cmd.type) {
		case CmdType::RETURN:
		{
			len = sizeof (unsigned short) + cmd.return_content.size ();

			std::array<unsigned char, sizeof (unsigned short)> len_byte_code = u_short_to_u_char (len);
			res.push_back (len_byte_code[0]);
			res.push_back (len_byte_code[1]);

			std::array<unsigned char, sizeof (unsigned short)> tmp_ptr = u_short_to_u_char (cmd.tmp_ptr);
			res.push_back (tmp_ptr[0]);
			res.push_back (tmp_ptr[1]);

			std::copy (cmd.return_content.begin (), cmd.return_content.end (), std::back_inserter (res));
		}
		break;
		case CmdType::ASSIGN:
		{
			len = 2 * sizeof (unsigned short);

			std::array<unsigned char, sizeof (unsigned short)> len_byte_code = u_short_to_u_char (len);
			res.push_back (len_byte_code[0]);
			res.push_back (len_byte_code[1]);

			std::array<unsigned char, sizeof (unsigned short)> static_ptr = u_short_to_u_char (cmd.static_ptr);
			res.push_back (static_ptr[0]);
			res.push_back (static_ptr[1]);

			std::array<unsigned char, sizeof (unsigned short)> tmp_ptr = u_short_to_u_char (cmd.vars[0].ptr);
			res.push_back (tmp_ptr[0]);
			res.push_back (tmp_ptr[1]);
		}
		break;
		case CmdType::FUNC:
		{
			len = 2 * sizeof (unsigned short) + cmd.vars.size () * (1 + sizeof (unsigned short));

			std::array<unsigned char, sizeof (unsigned short)> len_byte_code = u_short_to_u_char (len);
			res.push_back (len_byte_code[0]);
			res.push_back (len_byte_code[1]);

			std::array<unsigned char, sizeof (unsigned short)> func_byte_code = u_short_to_u_char (cmd.func_id);
			res.push_back (func_byte_code[0]);
			res.push_back (func_byte_code[1]);

			std::array<unsigned char, sizeof (unsigned short)> tmp_ptr = u_short_to_u_char (cmd.tmp_ptr);
			res.push_back (tmp_ptr[0]);
			res.push_back (tmp_ptr[1]);

			for (var_ptr& var : cmd.vars) {
				unsigned char vartype = static_cast<unsigned char>(var.type);
				res.push_back (vartype);

				std::array<unsigned char, sizeof (unsigned short)> var_ptr = u_short_to_u_char (var.ptr);
				res.push_back (var_ptr[0]);
				res.push_back (var_ptr[1]);
			}
		}
		break;
		default:
			throw "ToByteCode: Unknown CommandType";
			break;
		}
	}

	return res;
}

int main () {
	// Define functions
	function_map.emplace ("$concat", 1);
	function_map.emplace ("$print", 2);


	std::string content = read_file ("script.rbsl");
	std::string cleaned_content = clean_content (content);
	std::vector<std::string> sections = separate (cleaned_content);

	std::vector<Cmd> main_cmds;

	for (auto& sec : sections) {
		std::vector<Cmd> cmds = analyse_section (sec);
		main_cmds.insert (main_cmds.end (), cmds.begin (), cmds.end ());
	}

	std::vector<Cmd> sub_cmds;
	flatten_commands (main_cmds, sub_cmds);

	std::vector<CompiledCmd> compiled_cmds = compile (sub_cmds);

	// 1 Byte : Instruction Type
	// 2 Bytes: Body Length
	// BODY
	// RETURN: 2 Bytes Create Temporary Variable Pointer
	// ASSIGN: 2 Bytes Target Static Variable Pointer <-- 2 Bytes Source Temporary Variable Pointer
	// FUNC  : 2 Bytes Function Type + 2 Bytes Temporary Return Value Pointer + ParameterCount * (1 Byte Pointer Type + 2 Bytes Pointer)

	std::vector<unsigned char> byte_code = to_byte_code (compiled_cmds);

	write_file ("new_script.bc", byte_code);

	/*std::string line;
	std::ifstream input;
	std::vector<std::string> lines;
	std::vector<Variable> vars;

	input.open ("script.rbsl", std::ios::in);

	if (!input.is_open ()) {
		EXIT_FAILURE;
	}

	while (std::getline (input, line)) {
		lines.push_back (line);
	}

	input.close ();

	for (auto& line : lines) {
		if (line.size () < 1) {
			continue;
		}

		switch (line[0]) {
		case '#': // Variable
			read_var (line, vars);
			break;
		case '$': // Special something
			break;
		default:
			break;
		}
	}

	for (auto& var : vars) {
		std::cout << "Variable Name: \"" << var.name << "\" Value: \"" << var.value << "\"\n";
	}
	*/
	system ("pause");
}