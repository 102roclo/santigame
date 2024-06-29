
void os_write_string_to_stdout(string s);
inline int crt_sprintf(char *str, const char *format, ...);
int vsnprintf(char* buffer, size_t n, const char* fmt, va_list args);
bool is_pointer_valid(void *p);

u64 format_string_to_buffer(char* buffer, u64 count, const char* fmt, va_list args) {
	if (!buffer) count = UINT64_MAX;
    const char* p = fmt;
    char* bufp = buffer;
    while (*p != '\0' && (bufp - buffer) < count - 1) {
        if (*p == '%') {
            p += 1;
            if (*p == 's') {
            	// We replace %s formatting with our fixed length string
                p += 1;
                string s = va_arg(args, string);
                assert(s.count < (1024ULL*1024ULL*1024ULL*256ULL), "Ypu passed something else than a fixed-length 'string' to %%s. Maybe you passed a char* and should do %%cs instead?");
                for (u64 i = 0; i < s.count && (bufp - buffer) < count - 1; i++) {
                	if (buffer) *bufp = s.data[i];
                    bufp += 1;
                }
            } else if (*p == 'c' && *(p+1) == 's') {
            	// We extend the standard formatting and add %cs so we can format c strings if we need to
                p += 2;
                char* s = va_arg(args, char*);
                assert(is_pointer_valid(s), "You passed an invalid pointer to %%cs");
                u64 len = 0;
                while (*s != '\0' && (bufp - buffer) < count - 1) {
                	assert(is_pointer_valid(s) && len < (1024ULL*1024ULL*1024ULL*1ULL), "The argument passed to %%cs is either way too big, missing null-termination or simply not a char*.");
                	if (buffer) {
                		*bufp = *s;
                	}
                	s += 1;
                    bufp += 1;
                    len += 1;
                    assert(is_pointer_valid(s) && len < (1024ULL*1024ULL*1024ULL*1ULL), "The argument passed to %%cs is either way too big, missing null-termination or simply not a char*.");
                }
            } else {
                // Fallback to standard vsnprintf
                char temp_buffer[512];
                char format_specifier[64];
                int specifier_len = 0;
                format_specifier[specifier_len++] = '%';

                while (*p != '\0' && strchr("diuoxXfFeEgGaAcCpn%", *p) == NULL) {
                    format_specifier[specifier_len++] = *p++;
                }
                if (*p != '\0') {
                    format_specifier[specifier_len++] = *p++;
                }
                format_specifier[specifier_len] = '\0';

                int temp_len = vsnprintf(temp_buffer, sizeof(temp_buffer), format_specifier, args);
                switch (format_specifier[specifier_len - 1]) {
                    case 'd': case 'i': va_arg(args, int); break;
                    case 'u': case 'x': case 'X': case 'o': va_arg(args, unsigned int); break;
                    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': va_arg(args, double); break;
                    case 'c': va_arg(args, int); break;
                    case 's': va_arg(args, char*); break;
                    case 'p': va_arg(args, void*); break;
                    case 'n': va_arg(args, int*); break;
                    default: break;
                }

                if (temp_len < 0) {
                    return -1; // Error in formatting
                }

                for (int i = 0; i < temp_len && (bufp - buffer) < count - 1; i++) {
                    if (buffer) *bufp = temp_buffer[i];
                    bufp += 1;
                }
            }
        } else {
            if (buffer) {
                *bufp = *p;
            }
            bufp += 1;
            p += 1;
        }
    }
    if (buffer)  *bufp = '\0';
    
    return bufp - buffer;
}
string sprint_null_terminated_string_va_list_to_buffer(const char *fmt, va_list args, void* buffer, u64 count) {
    u64 formatted_length = format_string_to_buffer((char*)buffer, count, fmt, args);
    
    string result;
    result.data = (u8*)buffer;
    
    if (formatted_length >= 0 && formatted_length < count) {
        result.count = formatted_length; 
    } else {
        result.count = count - 1; 
    }

    return result;
}
string sprint_va_list_to_buffer(const string fmt, va_list args, void* buffer, u64 count) {
	
	char* fmt_cstring = temp_convert_to_null_terminated_string(fmt);
	return sprint_null_terminated_string_va_list_to_buffer(fmt_cstring, args, buffer, count);
}
// context.allocator
string sprint_va_list(const string fmt, va_list args) {

    char* fmt_cstring = temp_convert_to_null_terminated_string(fmt);
    u64 count = format_string_to_buffer(NULL, 0, fmt_cstring, args) + 1; 

    char* buffer = NULL;

    buffer = (char*)alloc(count);

    return sprint_null_terminated_string_va_list_to_buffer(fmt_cstring, args, buffer, count);
}

// context.allocator
string sprints(const string fmt, ...) {
	va_list args = 0;
	va_start(args, fmt);
	string s = sprint_va_list(fmt, args);
	va_end(args);
	return s;
}

// temp allocator
string tprints(const string fmt, ...) {
	va_list args = 0;
	va_start(args, fmt);
	push_temp_allocator();
	string s = sprint_va_list(fmt, args);
	pop_allocator();
	va_end(args);
	return s;
}

// context.allocator
string sprintf(const char *fmt, ...) {
	string sfmt;
	sfmt.data = cast(u8*)fmt;
	sfmt.count = strlen(fmt);
	
	va_list args;
	va_start(args, fmt);
	string s = sprint_va_list(sfmt, args);
	va_end(args);
	
	return s;
}
// temp allocator
string tprintf(const char *fmt, ...) {
	string sfmt;
	sfmt.data = cast(u8*)fmt;
	sfmt.count = strlen(fmt);
	
	va_list args;
	va_start(args, fmt);
	push_temp_allocator();
	string s = sprint_va_list(sfmt, args);
	pop_allocator();
	va_end(args);
	
	return s;
}


// context.allocator (alloc & dealloc)
void print_va_list(const string fmt, va_list args) {
	string s = sprint_va_list(fmt, args);
	os_write_string_to_stdout(s);
	dealloc(s.data);
}

// print for 'string' and printf for 'char*'

#define PRINT_BUFFER_SIZE 4096
// Avoids all and any allocations but overhead in speed and memory.
// Need this for standard printing so we don't get infinite recursions.
// (for example something in memory might fail assert and it needs to print that)
void print_va_list_buffered(const string fmt, va_list args) {

	string current = fmt;

	char buffer[PRINT_BUFFER_SIZE];
	
	while (true) {
		u64 size = min(current.count, PRINT_BUFFER_SIZE-1);
		if (current.count <= 0) break;
		
		memcpy(buffer, current.data, size);
		
		char fmt_cstring[PRINT_BUFFER_SIZE+1];
		memcpy(fmt_cstring, current.data, size);
		fmt_cstring[size] = 0;
		
		string s = sprint_null_terminated_string_va_list_to_buffer(fmt_cstring, args, buffer, PRINT_BUFFER_SIZE);
		os_write_string_to_stdout(s);
		
		current.count -= size;
		current.data += size;
	}
}


// context.allocator (alloc & dealloc)
void prints(const string fmt, ...) {
	va_list args;
	va_start(args, fmt);
	print_va_list_buffered(fmt, args);
	va_end(args);	
}
// context.allocator (alloc & dealloc)
void printf(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	string s;
	s.data = cast(u8*)fmt;
	s.count = strlen(fmt);
	print_va_list_buffered(s, args);
	va_end(args);
}

