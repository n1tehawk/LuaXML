-- unit tests for LuaXML

local lu = require('luaunit')
xml = require('LuaXML')

TestXml = {} -- the test suite

function TestXml:test_basics()
	-- encoding of 'signed' character
	lu.assertEquals(xml.encode("\128"), "&#128;")

	-- check metatable of newly created object
	local foobar = xml.new()
	lu.assertIsTable(foobar)
	lu.assertEquals(getmetatable(foobar).__index, xml)

	-- simple XML strings
	lu.assertNil(xml.eval(""))
	foobar = xml.eval('<foo bar="true" />')
	lu.assertIsTable(foobar)
	lu.assertEquals(foobar[0], "foo")
	lu.assertEquals(foobar.bar, "true")

	-- direct conversion (XML 'encapsulation') of Lua values
	lu.assertEquals(xml.str(true), "<boolean>true</boolean>\n")
	lu.assertEquals(xml.str(123), "<number>123</number>\n")
	lu.assertEquals(xml.str("bar"), "<string>bar</string>\n")

	-- construct new XML object in a piecemeal fashion, and call a method
	foobar = {bar = false}
	xml.tag(foobar, "foo")
	lu.assertEquals(xml.new(foobar):str(), '<foo bar="false" />\n')

	-- proper handling of an empty attribute
	local foo = '<tag attr="" />\n'
	foobar = xml.eval(foo)
	lu.assertEquals(foobar.attr, "")
	lu.assertEquals(foobar:str(), foo)

	-- encoding / decoding of special entities
	foo = xml.new({"<&>"})
	xml.tag(foo, "foo")
	lu.assertEquals(foo:str(), "<foo>&lt;&amp;&gt;</foo>\n")
	lu.assertEquals(xml.eval("<foo>&#032;</foo>")[1], " ")
	lu.assertEquals(xml.eval("<bar>&#32;</bar>")[1], " ")
	lu.assertEquals(xml.eval("<foobar>&apos;&#9;&apos;</foobar>")[1], "'\t'")

	-- invalid XML
	lu.assertErrorMsgContains("Malformed XML", xml.eval, "foo<bar/>")

	-- check load error
	lu.assertErrorMsgContains("file error or file not found",
		xml.load, "invalid_filename")

	-- safeguard against global namespace pollution
	--lu.assertNil(_G.xml)
end

function TestXml:test_parse()
	local test = xml.load("test.xml")

	local scene = test:find("scene")
	lu.assertIsTable(scene)
	lu.assertEquals(scene:tag(), "scene") -- XML tag
	lu.assertEquals(scene.id, "0") -- XML property/attribute
	lu.assertEquals(scene.script, "main")

	-- first (sub)element
	scene = scene[1]
	lu.assertIsTable(scene)
	lu.assertEquals(scene:tag(), "object")
	lu.assertEquals(scene.name, "observer")
	lu.assertEquals(scene.id, "0")
	lu.assertEquals(scene.input, "window")
	lu.assertEquals(scene.script, "camera.lua")

	-- make sure that the CDATA element from <script> starts with '\n' and
	-- ends with ' ' (i.e. preserved whitespace)
	local script_str = test:find("script")[1]
	lu.assertEquals(script_str:sub(1, 1), '\n')
	lu.assertEquals(script_str:sub(-1), ' ')
end

function TestXml:test_transform()
	local test = xml.load("test.xml")
	-- There is a slight inconsistency in the output produced by LuaXML:
	-- It doesn't preserve the CDATA encapsulation within the <script> element,
	-- which in turn causes xml.load() to discard whitespace enclosing the
	-- value. The two tables won't match unless we do the same for the original.
	local script = test:find("script")
	script[1] = string.gsub(script[1], "^[ \t\n]+", "")
	script[1] = string.gsub(script[1], "[ \t\n]+$", "")
	-- make sure we can parse back the textual representation without "losses"
	local expected = xml.eval(test:str())
	lu.assertEquals(test, expected)
end

-- run test suite with verbose output
os.exit(lu.LuaUnit.run("-v"))
