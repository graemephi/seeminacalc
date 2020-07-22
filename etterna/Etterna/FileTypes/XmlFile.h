/* XmlFile - Simple XML reading and writing. */

#ifndef XML_FILE_H
#define XML_FILE_H

struct DateTime;
class RageFileBasic;
struct lua_State;

class XNodeValue
{
  public:
	virtual ~XNodeValue() = default;
	[[nodiscard]] virtual auto Copy() const -> XNodeValue* = 0;

	virtual void GetValue(std::string& out) const = 0;
	virtual void GetValue(int& out) const = 0;
	virtual void GetValue(float& out) const = 0;
	virtual void GetValue(bool& out) const = 0;
	virtual void GetValue(unsigned& out) const = 0;
	virtual void PushValue(lua_State* L) const = 0;

	template<typename T>
	auto GetValue() const -> T
	{
		T val;
		GetValue(val);
		return val;
	}

	virtual void SetValue(const std::string& v) = 0;
	virtual void SetValue(int v) = 0;
	virtual void SetValue(float v) = 0;
	virtual void SetValue(unsigned v) = 0;
	virtual void SetValueFromStack(lua_State* L) = 0;
};

class XNodeStringValue : public XNodeValue
{
  public:
	std::string m_sValue;

	[[nodiscard]] auto Copy() const -> XNodeValue* override
	{
		return new XNodeStringValue(*this);
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

class XNode;
using XNodes = std::vector<XNode*>;

class XNode
{
  private:

  public:
	std::string m_sName;

	void SetName(const std::string& sName) { m_sName = sName; }
	[[nodiscard]] auto GetName() const -> const std::string& { return m_sName; }

	static const std::string TEXT_ATTRIBUTE;
	template<typename T>
	void GetTextValue(T& out) const
	{
		GetAttrValue(TEXT_ATTRIBUTE, out);
	}

	// in own attribute list
	[[nodiscard]] auto GetAttr(const std::string& sAttrName) const
	  -> const XNodeValue*;
	auto GetAttr(const std::string& sAttrName) -> XNodeValue*;
	template<typename T>
	auto GetAttrValue(const std::string& sName, T& out) const -> bool
	{
		const XNodeValue* pAttr = GetAttr(sName);
		if (pAttr == nullptr) {
			return false;
		}
		pAttr->GetValue(out);
		return true;
	}
	auto PushAttrValue(lua_State* L, const std::string& sName) const -> bool;


	// in one level child nodes
	[[nodiscard]] auto GetChild(const std::string& sName) const -> const XNode*;
	auto GetChild(const std::string& sName) -> XNode*;
	template<typename T>
	auto GetChildValue(const std::string& sName, T& out) const -> bool
	{
		const XNode* pChild = GetChild(sName);
		if (pChild == nullptr) {
			return false;
		}
		pChild->GetTextValue(out);
		return true;
	}
	auto PushChildValue(lua_State* L, const std::string& sName) const -> bool;

	// modify DOM
	template<typename T>
	auto AppendChild(const std::string& sName, T value) -> XNode*
	{
		XNode* p = AppendChild(sName);
		p->AppendAttr(XNode::TEXT_ATTRIBUTE, value);
		return p;
	}
	auto AppendChild(const std::string& sName) -> XNode*
	{
		auto* p = new XNode(sName);
		return AppendChild(p);
	}
	auto AppendChild(XNode* node) -> XNode*;
	auto RemoveChild(XNode* node, bool bDelete = true) -> bool;
	void RemoveChildFromByName(XNode* node);
	void RenameChildInByName(XNode* node);

	auto AppendAttrFrom(const std::string& sName,
						XNodeValue* pValue,
						bool bOverwrite = true) -> XNodeValue*;
	auto AppendAttr(const std::string& sName) -> XNodeValue*;
	template<typename T>
	auto AppendAttr(const std::string& sName, T value) -> XNodeValue*
	{
		XNodeValue* pVal = AppendAttr(sName);
		pVal->SetValue(value);
		return pVal;
	}
	auto RemoveAttr(const std::string& sName) -> bool;

	auto ChildrenEmpty() -> bool { return 0; }

	XNode();
	explicit XNode(const std::string& sName);
	XNode(const XNode& cpy);
	~XNode() { Free(); }

	void Clear();

  private:
	void Free();
	auto operator=(const XNode& cpy) -> XNode& = delete; // don't use
};

#endif

const std::string XNode::TEXT_ATTRIBUTE = "__TEXT__";

XNode::XNode() = default;

XNode::XNode(const std::string& sName)
{
	m_sName = sName;
}

XNode::XNode(const XNode& cpy)
  : m_sName(cpy.m_sName)
{
}

void
XNode::Clear()
{
	Free();
}

void
XNode::Free()
{
}

void
XNodeStringValue::GetValue(std::string& out) const
{
}
void
XNodeStringValue::GetValue(int& out) const
{
}
void
XNodeStringValue::GetValue(float& out) const
{
}
void
XNodeStringValue::GetValue(bool& out) const
{
}
void
XNodeStringValue::GetValue(unsigned& out) const
{
}
void
XNodeStringValue::PushValue(lua_State* L) const
{
}

void
XNodeStringValue::SetValue(const std::string& v)
{
}
void
XNodeStringValue::SetValue(int v)
{
}
void
XNodeStringValue::SetValue(float v)
{
}
void
XNodeStringValue::SetValue(unsigned v)
{
}
void
XNodeStringValue::SetValueFromStack(lua_State* L)
{
}

const XNodeValue*
XNode::GetAttr(const std::string& attrname) const
{
    return 0;
}

bool
XNode::PushAttrValue(lua_State* L, const std::string& sName) const
{
    return 0;
}

XNodeValue*
XNode::GetAttr(const std::string& attrname)
{
    return 0;
}

XNode*
XNode::GetChild(const std::string& sName)
{
    return 0;
}

bool
XNode::PushChildValue(lua_State* L, const std::string& sName) const
{
    return 0;
}

const XNode*
XNode::GetChild(const std::string& sName) const
{
    return 0;
}

XNode*
XNode::AppendChild(XNode* node)
{
    return 0;
}

// detach node and delete object
bool
XNode::RemoveChild(XNode* node, bool bDelete)
{
    return 0;
}

void
XNode::RemoveChildFromByName(XNode* node)
{
}

void
XNode::RenameChildInByName(XNode* node)
{
}

// detach attribute
bool
XNode::RemoveAttr(const std::string& sName)
{
    return 0;
}

/* If bOverwrite is true and a node already exists with that name, the old value
 * will be deleted. If bOverwrite is false and a node already exists with that
 * name, the new value will be deleted. */
XNodeValue*
XNode::AppendAttrFrom(const std::string& sName,
					  XNodeValue* pValue,
					  bool bOverwrite)
{
    return 0;
}

XNodeValue*
XNode::AppendAttr(const std::string& sName)
{
    return 0;
}
