/*
LuaXML License

LuaXML is licensed under the terms of the MIT license reproduced below,
the same as Lua itself. This means that LuaXML is free software and can be
used for both academic and commercial purposes at absolutely no cost.

Copyright (C) 2007-2013 Gerald Franz, eludi.net

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/// @module LuaXML

#include "LuaXML_lib.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* compatibility with older Lua versions (<5.2) */
#if LUA_VERSION_NUM < 502

	// Substitute lua_objlen() for lua_rawlen()
	#define lua_rawlen(L, index)	lua_objlen(L, index)

	// Make use of luaL_register() to achieve same result as luaL_newlib()
	#define luaL_newlib(L, funcs) do { \
		lua_newtable(L); \
		luaL_register(L, NULL, funcs); \
	} while (0)

#endif


#define LUAXML_META	"LuaXML" // name to be used for metatable

//--- auxliary functions -------------------------------------------

static size_t find(const char* s, const char* pattern, size_t start) {
	const char* found =strstr(s+start, pattern);
	return found ? found-s : strlen(s);
}

// push (arbitrary Lua) value to be used as tag key, placing it on top of stack
static inline void push_TAG_key(lua_State *L) {
	/* Note: Currently this is the number 0, which fits in nicely with using
	 * string keys for attribute-value pairs and also 'stays clear' of the
	 * array of sub-elements (starting at index 1).
	 * Theoretically, this could be any kind of Lua value; but when using a
	 * string key (e.g. "TAG"), extra care needs to be taken that it doesn't
	 * get confused with an attribute - which means that the str() function
	 * should be modified accordingly (to recognise and avoid the tag key).
	 */
	lua_pushinteger(L, 0);
}

// convert Lua table at given index to an XML "object", by setting its metatable
static void make_xml_object(lua_State *L, int index) {
	if (index < 0) index += lua_gettop(L) + 1; // relative to absolute index
	if (!lua_istable(L, index))
		luaL_error(L, "%s() error: invalid type at %d - expected table, got %s",
			__func__, index, lua_typename(L, lua_type(L, index)));

	luaL_getmetatable(L, LUAXML_META);

	lua_pushliteral(L, "__index");
	lua_rawget(L, -2); // retrieve the actual module table
	lua_pushliteral(L, "__tostring");
	lua_pushliteral(L, "str");
	lua_rawget(L, -3); // try to retrieve function via "str" key
	lua_remove(L, -3);
	lua_rawset(L, -3); // store function as __tostring metamethod

	lua_setmetatable(L, index); // assign metatable
}

/*
 * Lua C function to replace a gsub() match with the corresponding character.
 * Xml_pushDecode() will use this as a replacement function argument to undo
 * the XML encodings, passing one match (sequence of digits) at a time.
 *
 * Due to the pattern used, the matched string may also be 'x' followed by
 * a sequence of hexadecimal characters ("xE4"), which is supported too.
 */
static int XMLencoding_replacement(lua_State *L) {
	const char *matched = lua_tostring(L, 1);
	if (matched) {
		// support both decimal and hexadecimal conversion
		char c = *matched == 'x' ? strtol(++matched, NULL, 16) : atoi(matched);
		if (c) {
			lua_pushlstring(L, &c, 1); // return character as Lua string
			return 1;
		} // c == 0 probably indicates conversion failure, return `nil`
	}
	return 0;
}


// control chars used by the Tokenizer to denote special meanings
#define ESC	27	/* end of scope, closing tag */
#define OPN	28	/* "open", start of tag */
#define CLS	29	/* closes opening tag, actual content follows */

//--- internal tokenizer -------------------------------------------

typedef struct Tokenizer_s  {
	/// stores string to be tokenized
	const char* s;
	/// stores size of string to be tokenized
	size_t s_size;
	/// stores current read position
	size_t i;
	/// stores current read context
	int tagMode;
	/// stores next token, if already determined
	const char* m_next;
	/// size of next token
	size_t m_next_size;
	/// pointer to current token
	char* m_token;
	/// size of current token
	size_t m_token_size;
	/// capacity of current token
	size_t m_token_capacity;
} Tokenizer;

Tokenizer* Tokenizer_new(const char* str, size_t str_size) {
	Tokenizer *tok = calloc(1, sizeof(Tokenizer));
	tok->s_size = str_size;
	tok->s = str;
	return tok;
}

void Tokenizer_delete(Tokenizer* tok) {
	free(tok->m_token);
	free(tok);
}

#if LUAXML_DEBUG
void Tokenizer_print(Tokenizer *tok) {
	printf("  @%u %s\n", tok->i,
		!tok->m_token ? "(null)" :
		(tok->m_token[0] == ESC) ? "(esc)" :
		(tok->m_token[0] == OPN) ? "(open)" :
		(tok->m_token[0] == CLS) ? "(close)" :
		tok->m_token);
	fflush(stdout);
}
#else
# define Tokenizer_print(tok)	/* ignore */
#endif

static const char* Tokenizer_set(Tokenizer* tok, const char* s, size_t size) {
	if(!size||!s) return 0;
	free(tok->m_token);
	tok->m_token = malloc(size + 1);
	strncpy(tok->m_token,s, size);
	tok->m_token[size] = 0;
	tok->m_token_size = tok->m_token_capacity = size;
	Tokenizer_print(tok);
	return tok->m_token;
}

static void Tokenizer_append(Tokenizer* tok, char ch) {
	if(tok->m_token_size+1>=tok->m_token_capacity) {
		tok->m_token_capacity = (tok->m_token_capacity==0) ? 16 : tok->m_token_capacity*2;
		tok->m_token = realloc(tok->m_token, tok->m_token_capacity);
	}
	tok->m_token[tok->m_token_size]=ch;
	tok->m_token[++tok->m_token_size]=0;
}

const char* Tokenizer_next(Tokenizer* tok) {
	// NUL-terminated strings for the special tokens
	static const char ESC_str[] = {ESC, 0};
	static const char OPEN_str[] = {OPN, 0};
	static const char CLOSE_str[] = {CLS, 0};

	if(tok->m_token) {
		free(tok->m_token);
		tok->m_token = 0;
		tok->m_token_size=tok->m_token_capacity = 0;
	}

	int quotMode=0;
	int tokenComplete = 0;
	while(tok->m_next_size || (tok->i < tok->s_size)) {

		if(tok->m_next_size) {
			Tokenizer_set(tok, tok->m_next, tok->m_next_size);
			tok->m_next=0;
			tok->m_next_size=0;
			return tok->m_token;
		}

		switch(tok->s[tok->i]) {
			case '"':
			case '\'':
			if(tok->tagMode) {
				if(!quotMode) quotMode=tok->s[tok->i];
				else if(quotMode==tok->s[tok->i]) quotMode=0;
			}
			Tokenizer_append(tok, tok->s[tok->i]);
			break;
			case '<':
			if(!quotMode&&(tok->i+4<tok->s_size)&&(strncmp(tok->s+tok->i,"<!--",4)==0)) // strip comments
				tok->i=find(tok->s, "-->", tok->i+4)+2;
			else if(!quotMode&&(tok->i+9<tok->s_size)&&(strncmp(tok->s+tok->i,"<![CDATA[",9)==0)) { // interpet CDATA
				size_t b=tok->i+9;
				tok->i=find(tok->s, "]]>",b)+3;
				if(!tok->m_token_size) return Tokenizer_set(tok, tok->s+b, tok->i-b-3);
				tokenComplete = 1;
				tok->m_next = tok->s+b;
				tok->m_next_size = tok->i-b-3;
				--tok->i;
			}
			else if(!quotMode&&(tok->i+1<tok->s_size)&&((tok->s[tok->i+1]=='?')||(tok->s[tok->i+1]=='!'))) // strip meta information
				tok->i=find(tok->s, ">", tok->i+2);
			else if(!quotMode&&!tok->tagMode) {
				if((tok->i+1<tok->s_size)&&(tok->s[tok->i+1]=='/')) {
					tok->m_next=ESC_str;
					tok->m_next_size = 1;
					tok->i=find(tok->s, ">", tok->i+2);
				}
				else {
					tok->m_next = OPEN_str;
					tok->m_next_size = 1;
					tok->tagMode=1;
				}
				tokenComplete = 1;
			}
			else Tokenizer_append(tok, tok->s[tok->i]);
			break;
			case '/':
			if(tok->tagMode&&!quotMode) {
				tokenComplete = 1;
				if((tok->i+1 < tok->s_size) && (tok->s[tok->i+1]=='>')) {
					tok->tagMode=0;
					tok->m_next=ESC_str;
					tok->m_next_size = 1;
					++tok->i;
				}
				else Tokenizer_append(tok, tok->s[tok->i]);
			}
			else Tokenizer_append(tok, tok->s[tok->i]);
			break;
			case '>':
			if(!quotMode&&tok->tagMode) {
				tok->tagMode=0;
				tokenComplete = 1;
				tok->m_next = CLOSE_str;
				tok->m_next_size = 1;
			}
			else Tokenizer_append(tok, tok->s[tok->i]);
			break;
			case ' ':
			case '\r':
			case '\n':
			case '\t':
			if(tok->tagMode&&!quotMode) {
				if(tok->m_token_size) tokenComplete=1;
			}
			else if(tok->m_token_size) Tokenizer_append(tok, tok->s[tok->i]);
			break;
			default: Tokenizer_append(tok, tok->s[tok->i]);
		}
		++tok->i;
		if((tok->i>=tok->s_size)||(tokenComplete&&tok->m_token_size)) {
			tokenComplete=0;
			while(tok->m_token_size&&isspace(tok->m_token[tok->m_token_size-1])) // trim whitespace
				tok->m_token[--tok->m_token_size]=0;
			if(tok->m_token_size) break;
		}
	}
	Tokenizer_print(tok);
	return tok->m_token;
}

//--- local variables ----------------------------------------------

/// stores number of special character codes
static size_t sv_code_size=0;
/// stores currently allocated capacity for special character codes
static size_t sv_code_capacity=16;
/// stores code table for special characters
static char** sv_code=0;

//--- public methods -----------------------------------------------

static void Xml_pushDecode(lua_State *L, const char *s, int size) {
	if (size == 0) {
		lua_pushliteral(L, "");
		return;
	}
	if (size < 0) size = strlen(s);

	// try a gsub() substition of decimal and hexadecimal character encodings
	lua_pushlstring(L, s, size); // initial string
	lua_pushliteral(L, "gsub");
	lua_gettable(L, -2); // using string as object, retrieve the "gsub" function
	lua_insert(L, -2); // swap with function, making string the arg #1
	lua_pushliteral(L, "&#(x?%x+);"); // pattern for XML encodings (arg #2)
	lua_pushcfunction(L, XMLencoding_replacement); // replacement func (arg #3)
	lua_call(L, 3, 1); // three parameters, one result (the substituted string)

	size_t i;
	for(i=sv_code_size-1; i<sv_code_size; i-=2) {
		luaL_gsub(L, lua_tostring(L,-1), sv_code[i], sv_code[i-1]);
		lua_remove(L,-2);
	}
}

/** converts an XML string to a Lua table.
@function eval
@tparam string xmlstring  the string to be converted
@return  a Lua table containing the XML data, or `nil` in case of errors
*/
int Xml_eval(lua_State *L) {
	char* str = 0;
	size_t str_size=0;
	if(lua_isuserdata(L,1))
		str = lua_touserdata(L,1);
	else {
		const char * sTmp = luaL_checklstring(L,1,&str_size);
		str = malloc(str_size + 1);
		memcpy(str, sTmp, str_size);
		str[str_size]=0;
	}
	Tokenizer* tok = Tokenizer_new(str, str_size ? str_size : strlen(str));
	lua_settop(L,0);
	const char* token=0;
	int firstStatement = 1;
	while((token=Tokenizer_next(tok))!=0) if(token[0]==OPN) { // new tag found
		if(lua_gettop(L)) {
			lua_newtable(L);
			lua_pushvalue(L, -1); // duplicate table (keep one copy on stack)
			lua_rawseti(L, -3, lua_rawlen(L, -3) + 1);
		}
		else {
			if (firstStatement) {
				lua_newtable(L);
				firstStatement = 0;
			}
			else return lua_gettop(L);
		}
		make_xml_object(L, -1); // assign metatable

		// parse tag and content:
		push_TAG_key(L); // place tag key on top of stack
		lua_pushstring(L, Tokenizer_next(tok));
		lua_rawset(L, -3);

		while(((token = Tokenizer_next(tok))!=0)&&(token[0]!=CLS)&&(token[0]!=ESC)) { // parse tag header
			size_t sepPos=find(token, "=", 0);
			if(token[sepPos]) { // regular attribute
				const char* aVal =token+sepPos+2;
				lua_pushlstring(L, token, sepPos);
				Xml_pushDecode(L, aVal, strlen(aVal) - 1);
				lua_rawset(L, -3);
			}
		}
		if(!token||(token[0]==ESC)) {
			if(lua_gettop(L)>1) lua_settop(L,-2); // this tag has no content, only attributes
			else break;
		}
	}
	else if(token[0]==ESC) { // previous tag is over
		if(lua_gettop(L)>1) lua_settop(L,-2); // pop current table
		else break;
	}
	else { // read elements
		if (lua_gettop(L)) {
			Xml_pushDecode(L, token, -1);
			lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
		}
		else // stack is empty, i.e. we encountered the token *before* any tag
			luaL_error(L, "Malformed XML: non-empty string '%s' before any tag (parser pos %d)",
					   token, (int)tok->i);
	}
	Tokenizer_delete(tok);
	free(str);
	return lua_gettop(L);
}

/** loads XML data from a file and returns it as table.
Basically, this is just calling `eval` on the given file's content.

@function load
@tparam string filename  the name and path of the file to be loaded
@return  a Lua table representing the XML data, or `nil` in case of errors
*/
int Xml_load (lua_State *L) {
	const char * filename = luaL_checkstring(L,1);
	FILE * file=fopen(filename,"r");
	if(!file)
		return luaL_error(L,"LuaXML ERROR: \"%s\" file error or file not found!",filename);

	fseek (file , 0 , SEEK_END);
	size_t sz = ftell (file);
	rewind (file);
	char *buffer = malloc(sz + 1);
	sz = fread (buffer,1,sz,file);
	fclose(file);
	buffer[sz]=0;
	lua_pushlightuserdata(L,buffer);
	lua_replace(L,1);
	return Xml_eval(L);
};

/** registers a custom code for the conversion between non-standard characters
and XML character entities.

By default, only the most basic entities are known to LuaXML:
	" & < > '
ANSI codes above 127 are directly converted to the XML character codes of the
same number. If more character codes are needed, they can be registered using
this function.

@function registerCode
@tparam string decoded  the character (sequence) to be used within Lua
@tparam string encoded  the character entity to be used in XML
@see encode
*/
int Xml_registerCode(lua_State *L) {
	const char * decoded = luaL_checkstring(L,1);
	const char * encoded = luaL_checkstring(L,2);

	size_t i;
	for(i=0; i<sv_code_size; i+=2)
		if(strcmp(sv_code[i],decoded)==0)
			return luaL_error(L,"LuaXML ERROR: code already exists.");
	if(sv_code_size+2>sv_code_capacity) {
		sv_code_capacity*=2;
		sv_code = realloc(sv_code, sv_code_capacity * sizeof(char*));
	}
	sv_code[sv_code_size]=malloc(strlen(decoded) + 1);
	strcpy(sv_code[sv_code_size++], decoded);
	sv_code[sv_code_size]=malloc(strlen(encoded) + 1);
	strcpy(sv_code[sv_code_size++],encoded);
	return 0;
}

/** converts a string to XML encoding.
This function transforms` str` by replacing any special characters with
suitable XML encodings.

@function encode
@tparam string str  string to be transformed
@treturn string  the XML-encoded string
@see registerCode
*/
int Xml_encode(lua_State *L) {
	if(lua_gettop(L)!=1)
		return 0;
	luaL_checkstring(L,-1);
	// encode special entities
	size_t i;
	for(i=0; i<sv_code_size; i+=2) {
		luaL_gsub(L, lua_tostring(L,-1), sv_code[i], sv_code[i+1]);
		lua_remove(L,-2);
	}
	// transfer string one character at a time, encoding any chars with MSB set
	char buf[8];
	const unsigned char *s = (unsigned char *)lua_tostring(L, 1);
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	while (*s) {
		if (*s < 128)
			luaL_addchar(&b, *s); // copy character literally
		else {
			snprintf(buf, sizeof(buf), "&#%d;", *s); // encode char
			luaL_addstring(&b, buf);
		}
		s++;
	}
	luaL_pushresult(&b);
	lua_remove(L,-2);
	return 1;
}

#ifdef __cplusplus
extern "C" {
#endif
int _EXPORT luaopen_LuaXML_lib (lua_State* L) {
	static const struct luaL_Reg funcs[] = {
		{"load", Xml_load},
		{"eval", Xml_eval},
		{"encode", Xml_encode},
		{"registerCode", Xml_registerCode},
		{NULL, NULL}
	};
	luaL_newlib(L, funcs);

	// create a metatable for LuaXML "objects"
	luaL_newmetatable(L, LUAXML_META);
	lua_pushliteral(L, "__index");
	lua_pushvalue(L, -3); // duplicate the module table
	lua_rawset(L, -3); // and set it as metaindex
	lua_pop(L, 1); // drop value (metatable)

	// register default codes:
	if(!sv_code) {
		sv_code=malloc(sv_code_capacity * sizeof(char*));
		sv_code[sv_code_size++]="&";
		sv_code[sv_code_size++]="&amp;";
		sv_code[sv_code_size++]="<";
		sv_code[sv_code_size++]="&lt;";
		sv_code[sv_code_size++]=">";
		sv_code[sv_code_size++]="&gt;";
		sv_code[sv_code_size++]="\"";
		sv_code[sv_code_size++]="&quot;";
		sv_code[sv_code_size++]="'";
		sv_code[sv_code_size++]="&apos;";
	}
	return 1;
}
#ifdef __cplusplus
} // extern "C"
#endif
