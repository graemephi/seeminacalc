#ifndef XML_FILE_UTIL_H
#define XML_FILE_UTIL_H



class RageFileBasic;
class XNode;
struct lua_State;

/**
 * @brief A little graphic to the left of the song's text banner in the
 * MusicWheel.
 *
 * This is designed to help work with XML files. */
namespace XmlFileUtil {
auto
LoadFromFileShowErrors(XNode& xml, const std::string& sFile) -> bool;
auto
LoadFromFileShowErrors(XNode& xml, RageFileBasic& f) -> bool;

// Load/Save XML
void
Load(XNode* pNode, const std::string& sXml, std::string& sErrorOut);
auto
GetXML(const XNode* pNode, RageFileBasic& f, bool bWriteTabs = true) -> bool;
auto
GetXML(const XNode* pNode) -> std::string;
auto
SaveToFile(const XNode* pNode,
		   const std::string& sFile,
		   const std::string& sStylesheet = "",
		   bool bWriteTabs = true) -> bool;
auto
SaveToFile(const XNode* pNode,
		   RageFileBasic& f,
		   const std::string& sStylesheet = "",
		   bool bWriteTabs = true) -> bool;

void
AnnotateXNodeTree(XNode* pNode, const std::string& sFile);
void
CompileXNodeTree(XNode* pNode, const std::string& sFile);
auto
XNodeFromTable(lua_State* L) -> XNode*;

void
MergeIniUnder(XNode* pFrom, XNode* pTo);
} // namespace XmlFileUtil

#endif

#include <string>
#include <map>

using std::string;

bool
XmlFileUtil::LoadFromFileShowErrors(XNode& xml, RageFileBasic& f)
{
    return 0;
}

bool
XmlFileUtil::LoadFromFileShowErrors(XNode& xml, const std::string& sFile)
{
    return 0;
}

static void
InitEntities()
{
}

// skip spaces
static void
tcsskip(const std::string& s, std::string::size_type& i)
{
}

// put string of (psz~end) on ps string
static void
SetString(const std::string& s,
		  int iStart,
		  int iEnd,
		  std::string* ps,
		  bool trim = false)
{
}

// attr1="value1" attr2='value2' attr3=value3 />
//                                            ^- return pointer
// Desc   : loading attribute plain xml text
// Param  : pszAttrs - xml of attributes
//          pi = parser information
// Return : advanced string pointer. (error return npos)
namespace {
std::string::size_type
LoadAttributes(XNode* pNode,
			   const std::string& xml,
			   std::string& sErrorOut,
			   std::string::size_type iOffset)
{
    return {};
}

// <TAG attr1="value1" attr2='value2' attr3=value3 >
// </TAG>
// or
// <TAG />
//        ^- return pointer
// Desc   : load xml plain text
// Param  : pszXml - plain xml text
//          pi = parser information
// Return : advanced string pointer  (error return npos)
std::string::size_type
LoadInternal(XNode* pNode,
			 const std::string& xml,
			 std::string& sErrorOut,
			 std::string::size_type iOffset)
{
    return 0;
}

bool
GetXMLInternal(const XNode* pNode,
			   RageFileBasic& f,
			   bool bWriteTabs,
			   int& iTabBase)
{
    return 0;
}
} // namespace

void
XmlFileUtil::Load(XNode* pNode, const std::string& sXml, std::string& sErrorOut)
{
}

bool
XmlFileUtil::GetXML(const XNode* pNode, RageFileBasic& f, bool bWriteTabs)
{
    return 0;
}

std::string
XmlFileUtil::GetXML(const XNode* pNode)
{
    return {};
}

bool
XmlFileUtil::SaveToFile(const XNode* pNode,
						RageFileBasic& f,
						const std::string& sStylesheet,
						bool bWriteTabs)
{
    return 0;
}

bool
XmlFileUtil::SaveToFile(const XNode* pNode,
						const std::string& sFile,
						const std::string& sStylesheet,
						bool bWriteTabs)
{
    return 0;
}

class XNodeLuaValue : public XNodeValue
{
  public:
	XNodeValue* Copy() const override { return new XNodeLuaValue(*this); }

	template<typename T>
	T GetValue() const
	{
		T val;
		GetValue(val);
		return val;
	}

	void GetValue(std::string& out) const override;
	void GetValue(int& out) const override;
	void GetValue(float& out) const override;
	void GetValue(bool& out) const override;
	void GetValue(unsigned& out) const override;
	void PushValue(lua_State* L) const override;

	void SetValue(const std::string& v) override;
	void SetValue(int v) override;
	void SetValue(float v) override;
	void SetValue(unsigned v) override;
	void SetValueFromStack(lua_State* L) override;
};

void
XNodeLuaValue::PushValue(lua_State* L) const
{
}

void
XNodeLuaValue::GetValue(std::string& out) const
{
}
void
XNodeLuaValue::GetValue(int& out) const
{
}
void
XNodeLuaValue::GetValue(float& out) const
{
}
void
XNodeLuaValue::GetValue(bool& out) const
{
}
void
XNodeLuaValue::GetValue(unsigned& out) const
{
}

void
XNodeLuaValue::SetValueFromStack(lua_State* L)
{
}

void
XNodeLuaValue::SetValue(const std::string& v)
{
}

void
XNodeLuaValue::SetValue(int v)
{
}
void
XNodeLuaValue::SetValue(float v)
{
}
void
XNodeLuaValue::SetValue(unsigned v)
{
}

struct Lua;
namespace {
XNodeValue*
CompileXMLNodeValue(Lua* L,
					const std::string& sName,
					const XNodeValue* pValue,
					const std::string& sFile)
{
    return 0;
}
} // namespace

void
XmlFileUtil::AnnotateXNodeTree(XNode* pNode, const std::string& sFile)
{
}

void
XmlFileUtil::CompileXNodeTree(XNode* pNode, const std::string& sFile)
{
}

/* Pop a table off of the stack, and return an XNode tree referring recursively
 * to entries in the table.
 *
 * The table may not contain table cycles; if a cycle is detected, only the
 * first table seen will have a corresponding XNode.
 *
 * Users of the resulting XNode may access the original table via PushValue. */
XNode*
XmlFileUtil::XNodeFromTable(lua_State* L)
{
    return 0;
}

/* Move nodes from pFrom into pTo which don't already exist in pTo. For
 * efficiency, nodes will be moved, not copied, so pFrom will be modified.
 * On return, the contents of pFrom will be undefined and should be deleted. */
void
XmlFileUtil::MergeIniUnder(XNode* pFrom, XNode* pTo)
{
}
