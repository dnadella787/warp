#include "codegen/yaml_document_parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "codegen/spec_parser.hpp"

namespace warp::codegen::detail {

namespace {

struct source_line {
	std::size_t line {0};
	std::size_t indent {0};
	std::string text;
};

// all_of is vacuously true if std::string_view text is "" here
bool is_blank(std::string_view text) {
	return std::all_of(text.begin(), text.end(),
	                   [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
}

std::string trim(std::string_view text) {
	std::size_t begin = 0;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
		++begin;
	}
	std::size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
		--end;
	}
	return std::string(text.substr(begin, end - begin));
}

// remove comment from a line as long as it's not wrapped within single or double quotes{
std::string strip_comment(std::string_view text) {
	bool single_quoted = false;
	bool double_quoted = false;
	for (std::size_t index = 0; index < text.size(); ++index) {
		const char c = text[index];
		// if its ' and not within a double quote, we flip the single
		// quote or not i.e. 'hello -> true hello' -> false, double quote check
		// bc of "it's alright"
		if (c == '\'' && !double_quoted) {
			single_quoted = !single_quoted;
			continue;
		}
		// same using double quotes here
		if (c == '"' && !single_quoted) {
			double_quoted = !double_quoted;
			continue;
		}
		// the curr char is # (indicating comment) and we are not
		// in any quotes, in which case we strip off the comment and part in front
		if (c == '#' && !single_quoted && !double_quoted) {
			return std::string(text.substr(0, index));
		}
	}
	return std::string(text);
}

std::vector<source_line> tokenize_lines(std::string_view yaml_text) {
	std::vector<source_line> lines;
	std::size_t line_number = 1;
	std::size_t start = 0;
	while (start <= yaml_text.size()) {
		// find the line discriminator (i.e. newline char)
		const auto end = yaml_text.find('\n', start);
		// get a view of the line using the start and end markers
		auto raw = yaml_text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		// if this line is not empty and it's a CR character at the end, we strip the CR off
		// don't have to worry about \n because we grab the segment right till \n
		if (!raw.empty() && raw.back() == '\r') {
			raw.remove_suffix(1);
		}

		// remove comment from line
		const auto uncommented = strip_comment(raw);
		if (!is_blank(uncommented)) {
			std::size_t indent = 0;
			while (indent < uncommented.size() && uncommented[indent] == ' ') {
				++indent;
			}

			if (indent < uncommented.size() && uncommented[indent] == '\t') {
				throw spec_error(line_number, indent + 1, "tabs are not supported in YAML indentation");
			}
			lines.push_back(source_line {
			    .line = line_number,
			    .indent = indent,
			    .text = std::string(uncommented.substr(indent)),
			});
		}

		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
		++line_number;
	}
	return lines;
}

std::size_t find_mapping_separator(std::string_view text) {
	bool single_quoted = false;
	bool double_quoted = false;
	for (std::size_t index = 0; index < text.size(); ++index) {
		const char c = text[index];
		if (c == '\'' && !double_quoted) {
			single_quoted = !single_quoted;
		} else if (c == '"' && !single_quoted) {
			double_quoted = !double_quoted;
		} else if (c == ':' && !single_quoted && !double_quoted) {
			return index;
		}
	}
	return std::string_view::npos;
}

std::string parse_scalar_text(std::string text) {
	text = trim(text);
	if (text.size() >= 2 &&
	    ((text.front() == '"' && text.back() == '"') || (text.front() == '\'' && text.back() == '\''))) {
		text = text.substr(1, text.size() - 2);
	}
	return text;
}

bool has_key(const std::vector<std::pair<std::string, yaml_node>> &entries, std::string_view key) {
	return std::any_of(entries.begin(), entries.end(),
	                   [key](const auto &entry) { return std::string_view(entry.first) == key; });
}

void reject_duplicate_key(const std::vector<std::pair<std::string, yaml_node>> &entries, std::string_view key,
                          source_span span) {
	if (has_key(entries, key)) {
		throw spec_error(span, "spec.duplicate_key", "duplicate key '" + std::string(key) + "'");
	}
}

class yaml_parser {
public:
	explicit yaml_parser(std::vector<source_line> lines) : lines_(std::move(lines)) {
	}

	[[nodiscard]] yaml_node parse_document() {
		if (lines_.empty()) {
			return yaml_node {.type = yaml_node::kind::map, .line = 1, .column = 1};
		}
		std::size_t index = 0;
		auto document = parse_block(index, lines_[0].indent);
		if (index != lines_.size()) {
			throw spec_error(lines_[index].line, lines_[index].indent + 1, "unexpected trailing YAML content");
		}
		return document;
	}

private:
	[[nodiscard]] yaml_node parse_block(std::size_t &index, std::size_t indent) {
		if (index >= lines_.size()) {
			throw spec_error(lines_.back().line, lines_.back().indent + 1, "expected nested YAML block");
		}
		if (lines_[index].indent < indent) {
			throw spec_error(lines_[index].line, lines_[index].indent + 1, "invalid indentation");
		}
		// if the line is either of:
		// -
		// - hi
		// then we parse as a list
		// but we check '- ' otherwise we would parse --bob or -- bob as lists too
		if (lines_[index].text == "-" || lines_[index].text.starts_with("- ")) {
			return parse_list(index, indent);
		}
		// otherwise we parse it as a map
		return parse_map(index, indent);
	}

	[[nodiscard]] yaml_node parse_map(std::size_t &index, std::size_t indent) {
		yaml_node node {.type = yaml_node::kind::map, .line = lines_[index].line, .column = indent + 1};
		while (index < lines_.size()) {
			const auto &line = lines_[index];
			if (line.indent < indent) {
				break;
			}
			if (line.indent > indent) {
				throw spec_error(line.line, line.indent + 1, "unexpected indentation inside mapping");
			}
			if (line.text.rfind("- ", 0) == 0 || line.text == "-") {
				throw spec_error(line.line, line.indent + 1, "list item cannot appear at mapping scope");
			}

			const auto separator = find_mapping_separator(line.text);
			if (separator == std::string_view::npos) {
				throw spec_error(line.line, line.indent + 1, "expected 'key: value' mapping entry");
			}

			const std::string key = trim(std::string_view(line.text).substr(0, separator));
			if (key.empty()) {
				throw spec_error(line.line, line.indent + 1, "mapping key cannot be empty");
			}
			reject_duplicate_key(node.map_values, key, {.line = line.line, .column = line.indent + 1});

			const std::string remainder = trim(std::string_view(line.text).substr(separator + 1));
			++index;

			yaml_node value;
			if (!remainder.empty()) {
				value.type = yaml_node::kind::scalar;
				value.scalar = parse_scalar_text(remainder);
				value.line = line.line;
				value.column = line.indent + separator + 2;
			} else {
				if (index >= lines_.size() || lines_[index].indent <= indent) {
					throw spec_error(line.line, line.indent + separator + 1, "expected nested block after mapping key");
				}
				value = parse_block(index, lines_[index].indent);
			}
			node.map_values.emplace_back(key, std::move(value));
		}
		return node;
	}

	[[nodiscard]] yaml_node parse_list(std::size_t &index, std::size_t indent) {
		yaml_node node {.type = yaml_node::kind::list, .line = lines_[index].line, .column = indent + 1};
		while (index < lines_.size()) {
			const auto &line = lines_[index];
			if (line.indent < indent) {
				break;
			}
			if (line.indent > indent) {
				throw spec_error(line.line, line.indent + 1, "unexpected indentation inside list");
			}
			if (!(line.text == "-" || line.text.starts_with("- "))) {
				break;
			}

			const std::string remainder = line.text == "-" ? std::string {} : line.text.substr(2);
			++index;

			if (remainder.empty()) {
				if (index >= lines_.size() || lines_[index].indent <= indent) {
					throw spec_error(line.line, line.indent + 1, "expected nested block after list item");
				}
				node.list_values.push_back(parse_block(index, lines_[index].indent));
				continue;
			}

			const auto separator = find_mapping_separator(remainder);
			if (separator != std::string_view::npos) {
				yaml_node item {.type = yaml_node::kind::map, .line = line.line, .column = indent + 1};
				const std::string key = trim(std::string_view(remainder).substr(0, separator));
				if (key.empty()) {
					throw spec_error(line.line, line.indent + 3, "list item mapping key cannot be empty");
				}
				reject_duplicate_key(item.map_values, key, {.line = line.line, .column = line.indent + 3});
				const std::string value_text = trim(std::string_view(remainder).substr(separator + 1));
				if (!value_text.empty()) {
					item.map_values.emplace_back(key, yaml_node {
					                                      .type = yaml_node::kind::scalar,
					                                      .scalar = parse_scalar_text(value_text),
					                                      .line = line.line,
					                                      .column = indent + separator + 4,
					                                  });
				} else {
					if (index >= lines_.size() || lines_[index].indent <= indent) {
						throw spec_error(line.line, line.indent + separator + 3,
						                 "expected nested block after mapping key");
					}
					item.map_values.emplace_back(key, parse_block(index, lines_[index].indent));
				}

				if (index < lines_.size() && lines_[index].indent > indent) {
					auto continuation = parse_map(index, lines_[index].indent);
					for (auto &entry : continuation.map_values) {
						reject_duplicate_key(item.map_values, entry.first, span_of(entry.second));
						item.map_values.push_back(std::move(entry));
					}
				}

				node.list_values.push_back(std::move(item));
				continue;
			}

			node.list_values.push_back(yaml_node {
			    .type = yaml_node::kind::scalar,
			    .scalar = parse_scalar_text(remainder),
			    .line = line.line,
			    .column = indent + 3,
			});
		}
		return node;
	}

	std::vector<source_line> lines_;
};

} // namespace

source_span span_of(const yaml_node &node) {
	return source_span {.line = node.line, .column = node.column};
}

yaml_node parse_yaml_document(std::string_view yaml_text) {
	yaml_parser parser(tokenize_lines(yaml_text));
	return parser.parse_document();
}

} // namespace warp::codegen::detail
