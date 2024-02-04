#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <system_error>
#include <stdexcept>
#include <vector>

inline int open_file(const char * filename) {
	int fd = ::open(filename, O_RDONLY);
	if (fd < 0) {
		throw std::system_error(errno, std:: system_category());
	}
	return fd;
}

inline int create_file(const char * filename) {
	int fd = ::open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		throw std::system_error(errno, std:: system_category());
	}
	return fd;
}

struct filereader {
	inline filereader(const char * filename) : m_fd(open_file(filename)), m_offset(0) {}
	inline filereader(const std::string & filename) : m_fd(open_file(filename.c_str())) {}
	inline ~filereader() { ::close(m_fd); }

	inline void read(void * buffer, const size_t amount) {
		if (::read(m_fd, buffer, amount) == ssize_t(amount)) {
			m_offset += amount;
		} else {
			throw std::system_error(errno, std:: system_category());
		}
	}

	template<typename T>
	inline void read(T & buffer) {
		return read(&buffer, sizeof(T));
	}

	template<typename T>
	inline T read() {
		T tmp;
		read(&tmp, sizeof(T));
		return tmp;
	}

	inline std::string read_string(const size_t length) {
		std::string tmp(length, 0);
		read(&tmp[0], length);
		return tmp;
	}

	inline void seek(const size_t offset) {
		if (lseek(m_fd, offset, SEEK_SET) >= 0) {
			m_offset = offset;
		} else {
			throw std::system_error(errno, std:: system_category());
		}
	}

	inline void skip(const size_t amount) {
		if (lseek(m_fd, amount, SEEK_CUR) >= 0) {
			m_offset += amount;
		} else {
			throw std::system_error(errno, std:: system_category());
		}
	}

	inline size_t offset() const noexcept {
		return m_offset;
	}

	int m_fd;
	size_t m_offset;
};

struct filewriter {
	inline filewriter(const char * filename) : fd(create_file(filename)) {}
	inline filewriter(const std::string & filename) : fd(create_file(filename.c_str())) {}
	inline ~filewriter() { ::close(fd); }

	inline void write(const void * buffer, const size_t amount) {
		if (::write(fd, buffer, amount) != ssize_t(amount)) {
			throw std::system_error(errno, std:: system_category());
		}
	}

	int fd;
};

inline void make_directory(const std::string & filename) {
	for (
		size_t i = 0, end = filename.find('/', 0);
		end != std::string::npos;
		i = end + 1, end = filename.find('/', i)
	) {
		const std::string path(filename, 0, end);
		if (mkdir(path.c_str(), 0755) != 0) {
			if (errno != EEXIST) {
				throw std::system_error(errno, std:: system_category());
			}
		}
	}
}

void unpack_sprite(filereader & input, const uint32_t offset) {
	input.seek(offset);
	const auto name_offset = input.read<uint32_t>();
	const auto width = input.read<int32_t>();
	const auto height = input.read<int32_t>();
	input.skip(64); // margins, other stuff
	const auto num_textures = input.read<uint32_t>();
	std::vector<uint32_t> texture_offsets(num_textures);
	input.read(texture_offsets.data(), sizeof(uint32_t) * num_textures);
	input.seek(name_offset - 4);
	const auto name = input.read_string(input.read<uint32_t>());
	printf("sprite %s with %u textures of %dx%d:\n", name.c_str(), num_textures, width, height);
	for (uint32_t i = 0; i < num_textures; ++i) {
		//~ printf("tpag @ %u\n", texture_offsets[i]);
	}
}

auto texture_data_offset(filereader & input, const uint32_t offset) {
	input.seek(offset);
	const auto unknown1 = input.read<uint32_t>();
	const auto unknown2 = input.read<uint32_t>();
	const auto data_offset = input.read<uint32_t>();
	printf("fileinfo @ 0x%08x (%u, %u, 0x%08x)\n", offset, unknown1, unknown2, data_offset);
	return data_offset;
}

//~ string 024815f3 uni/fri/tomboy/toilet03.jpg, no refs
// PNG @ 0x24c7600, ref @ 0x5dc3d62 (IDAT?)
// PNG @ 0x2546b80, ref @ 0x24c6dd8 (TXTR), 0x1467f8ff, 0x2fe90c93
// PNG @ 0x2bb2280, ref @ 0x24c6de4 (TXTR)
//
//

void unpack_string(filereader & input, const uint32_t offset) {
	input.seek(offset);
	const auto value = input.read_string(input.read<uint32_t>());
	printf("string %08x %s\n", offset, value.c_str());
}

void unpack_chunk(filereader & input) {
	const auto magic = input.read<uint32_t>();
	const auto size = input.read<uint32_t>();

	printf("%4.4s %08x %u @ 0x%08x\n", &magic, magic, size, input.offset() - 8);

	switch (magic) {
		case 0x384e4547: return input.skip(size); // GEN8
		case 0x4e54504f: return input.skip(size); // OPTN
		case 0x474e414c: return input.skip(size); // LANG
		case 0x4e545845: return input.skip(size); // EXTN
		case 0x444e4f53: return input.skip(size); // SOND
		case 0x50524741: return input.skip(size); // AGRP
		case 0x54525053: { // SPRT
			const auto next = input.offset() + size;
			const auto num_sprites = input.read<uint32_t>();
			std::vector<uint32_t> offsets(num_sprites);
			input.read(offsets.data(), sizeof(uint32_t) * num_sprites);
			for (uint32_t i = 0; i < num_sprites; ++i) {
				unpack_sprite(input, offsets[i]);
			}
			return input.seek(next);
		}
		case 0x444e4742: { // BGND
			return input.skip(size);
		}
		case 0x48544150: return input.skip(size); // PATH
		case 0x54504353: return input.skip(size); // SCPT
		case 0x424f4c47: return input.skip(size); // GLOB
		case 0x52444853: return input.skip(size); // SHDR
		case 0x544e4f46: return input.skip(size); // FONT
		case 0x4e4c4d54: return input.skip(size); // TMLN
		case 0x544a424f: return input.skip(size); // OBJT
		case 0x4d4f4f52: return input.skip(size); // ROOM
		case 0x4c464144: return input.skip(size); // DAFL
		case 0x49424d45: return input.skip(size); // EMBI
		case 0x47415054: { // TPAG
			return input.skip(size);
		}
		case 0x4e494754: return input.skip(size); // TGIN
		case 0x45444f43: return input.skip(size); // CODE
		case 0x49524156: return input.skip(size); // VARI
		case 0x434e5546: return input.skip(size); // FUNC
		case 0x47525453: { // STRG
			const auto next = input.offset() + size;
			const auto num_strings = input.read<uint32_t>();
			std::vector<uint32_t> offsets(num_strings);
			input.read(offsets.data(), sizeof(uint32_t) * num_strings);
			for (uint32_t i = 0; i < num_strings; ++i) {
				unpack_string(input, offsets[i]);
			}
			return input.seek(next);
		}
		case 0x52545854: { // TXTR
			const auto next = input.offset() + size;
			const auto num_textures = input.read<uint32_t>();
			std::vector<uint32_t> offsets(num_textures + 1);
			input.read(offsets.data(), sizeof(uint32_t) * num_textures);
			for (uint32_t i = 0; i < num_textures; ++i) {
				offsets[i] = texture_data_offset(input, offsets[i]);
			}
			offsets[num_textures] = next;
			for (uint32_t i = 0; i < num_textures; ++i) {
				input.seek(offsets[i]);
				const auto len = offsets[i+1] - offsets[i];
				std::vector<char> buffer(len);
				input.read(buffer.data(), len);
				char filename[9];
				sprintf(filename, "%04d.png", i);
				auto out = filewriter(filename);
				out.write(buffer.data(), len);
			}
			return input.seek(next);
		}
		case 0x4f445541: return input.skip(size); // AUD0
		default:
			throw std::runtime_error("Unknown chunk");
	}
}

void unpack_form(filereader & input) {
	const auto magic = input.read<uint32_t>();
	const auto size = input.read<uint32_t>();

	if (magic == 0x4d524f46) { // FORM
		while (input.offset() < size) {
			unpack_chunk(input);
		}
	} else {
		throw std::runtime_error("Invalid input file");
	}
}

void unpack(const char * filename) {
	filereader input(filename);
	unpack_form(input);
}

int main(int argc, char ** argv) {
	if (argc >= 2) {
		unpack(argv[1]);
	}
	return 0;
}
