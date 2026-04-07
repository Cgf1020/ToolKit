#include "variant.h"

#include <iostream>
#include <sstream>

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
using namespace rapidjson;

namespace CommandSpace
{
    //Variant -> string 时候用到的
    bool filljsonstr(Writer<StringBuffer>& writer, Variant& val)
    {
        switch (val.getType())
        {
        case V_NULL:
        {
            writer.String("");
            break;
        }
        case V_BOOL:
        {
            writer.Bool(val);
            break;
        }
        case V_INT8:
        {
            writer.Int(val);
            break;
        }
        case V_INT16:
        {
            writer.Int(val);
            break;
        }
        case V_INT32:
        {
            writer.Int(val);
            break;
        }
        case V_INT64:
        {
            writer.Int64(val);
            break;
        }
        case V_UINT8:
        {
            writer.Uint(val);
            break;
        }
        case V_UINT16:
        {
            writer.Uint(val);
            break;
        }
        case V_UINT32:
        {
            writer.Uint(val);
            break;
        }
        case V_UINT64:
        {
            writer.Uint64(val);
            break;
        }
        case V_DOUBLE:
        {
            writer.Double(val);
            break;
        }
        case V_STRING:
        {
            writer.String(((std::string)val).c_str());
            break;
        }
        case V_MAP:
        {
            writer.StartObject();

            VariantMap varMap = val.toMap();

            for (auto it = varMap.begin(); it != varMap.end(); it++)
            {
                writer.Key(it->first.c_str());
                filljsonstr(writer, it->second);
            }
            writer.EndObject();
            break;
        }
        case V_ARRAY:
        {
            writer.StartArray();

            std::vector<Variant> vec = val;

            for (auto it = vec.begin(); it != vec.end(); it++)
            {
                filljsonstr(writer, *it);
            }

            writer.EndArray();
        }
        default:   
        {

        }
        }

        return true;
    }


	Variant::Variant()
		: _type(V_NULL)
		//, _value{0}
        , typeStack(NULL)
        , varStack(NULL)
        , keyStack(NULL)
	{
        //typeStack = NULL;
        //varStack = NULL;
        //keyStack = NULL;
		//_type = V_NULL;
		memset(&_value, 0, sizeof(_value));
	}

	Variant::Variant(VariantType type)
        : Variant()
	{
		_type = type;
		//memset(&_value, 0, sizeof(_value));
	}

	Variant::Variant(const Variant& val)
        : Variant()
	{
        internalCopy(val);
	}

    Variant::Variant(const bool& val)
        : Variant()
    {
        _type = V_BOOL;
        //memset(&_value, 0, sizeof(_value));
        _value.b = val;
    }

    Variant::Variant(const int8_t& val)
        : Variant()
    {
        _type = V_INT8;
        //memset(&_value, 0, sizeof(_value));
        _value.i8 = val;
    }

    Variant::Variant(const int16_t& val)
        : Variant()
    {
        _type = V_INT16;
        //memset(&_value, 0, sizeof(_value));
        _value.i16 = val;
    }

    Variant::Variant(const int32_t& val)
        : Variant()
    {
        _type = V_INT32;
        //memset(&_value, 0, sizeof(_value));
        _value.i32 = val;
    }

    Variant::Variant(const int64_t& val)
        : Variant()
    {
        _type = V_INT64;
        //memset(&_value, 0, sizeof(_value));
        _value.i64 = val;
    }

    Variant::Variant(const uint8_t& val)
        : Variant()
    {
        _type = V_UINT8;
        //memset(&_value, 0, sizeof(_value));
        _value.ui8 = val;
    }

    Variant::Variant(const uint16_t& val)
        : Variant()
    {
        _type = V_UINT16;
        //memset(&_value, 0, sizeof(_value));
        _value.ui16 = val;
    }

    Variant::Variant(const uint32_t& val)
        : Variant()
    {
        _type = V_UINT32;
        //memset(&_value, 0, sizeof(_value));
        _value.ui32 = val;
    }

    Variant::Variant(const uint64_t& val)
        : Variant()
    {
        _type = V_UINT64;
        //memset(&_value, 0, sizeof(_value));
        _value.ui64 = val;
    }

    Variant::Variant(const double& val)
        : Variant()
    {
        _type = V_DOUBLE;
        //memset(&_value, 0, sizeof(_value));
        _value.d = val;
    }

    Variant::Variant(const char* val)
        : Variant()
    {
        _type = V_STRING;
        //memset(&_value, 0, sizeof(_value));
        _value.s = new std::string(val);
    }

    Variant::Variant(const std::string& val)
        : Variant()
    {
        _type = V_STRING;
        //memset(&_value, 0, sizeof(_value));
        _value.s = new std::string(val);
    }

    Variant::Variant(const std::vector<Variant>& val)
        : Variant()
    {
        _type = V_ARRAY;
        //memset(&_value, 0, sizeof(_value));
        _value.l = new std::vector<Variant>(val);
    }

    Variant::Variant(const std::map<std::string, Variant>& val)
        : Variant()
    {
        _type = V_MAP;
        //memset(&_value, 0, sizeof(_value));
        _value.m = new std::map<std::string, Variant>(val);
    }

    Variant::~Variant()
    {
    }

    Variant& Variant::operator=(const Variant& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        internalCopy(val);
        return *this;
    }

    Variant& Variant::operator=(const bool& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_BOOL;
        _value.b = val;
        return *this;
    }

    Variant& Variant::operator=(const int8_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_INT8;
        _value.i8 = val;
        return *this;
    }

    Variant& Variant::operator=(const int16_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_INT16;
        _value.i16 = val;
        return *this;
    }

    Variant& Variant::operator=(const int32_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_INT32;
        _value.i32 = val;
        return *this;
    }

    Variant& Variant::operator=(const int64_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_INT64;
        _value.i64 = val;
        return *this;
    }

    Variant& Variant::operator=(const uint8_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_UINT8;
        _value.ui8 = val;
        return *this;
    }

    Variant& Variant::operator=(const uint16_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_UINT16;
        _value.ui16 = val;
        return *this;
    }

    Variant& Variant::operator=(const uint32_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_UINT32;
        _value.ui32 = val;
        return *this;
    }

    Variant& Variant::operator=(const uint64_t& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_UINT64;
        _value.ui64 = val;
        return *this;
    }

    Variant& Variant::operator=(const double& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_DOUBLE;
        _value.d = val;
        return *this;
    }

    Variant& Variant::operator=(const char* val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_STRING;
        _value.s = new std::string(val);
        return *this;
    }

    Variant& Variant::operator=(const std::string& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_STRING;
        _value.s = new std::string(val);
        return *this;
    }

    Variant& Variant::operator=(const std::vector<Variant>& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_ARRAY;
        _value.l = new std::vector<Variant>(val);
        return *this;
    }

    Variant& Variant::operator=(const std::map<std::string, Variant>& val)
    {
        // TODO: 在此处插入 return 语句
        clear();
        _type = V_MAP;
        _value.m = new std::map<std::string, Variant>(val);
        return *this;
    }

    Variant::operator VariantType()
    {
        return _type;
    }

    Variant::operator bool()
    {
        switch (_type) {
        case V_NULL:
        {
            return false;
            break;
        }
        case V_BOOL:
        {
            return _value.b;
            break;
        }
        case V_INT8:
        case V_INT16:
        case V_INT32:
        case V_INT64:
        case V_UINT8:
        case V_UINT16:
        case V_UINT32:
        case V_UINT64:
        case V_DOUBLE:
        {
            bool result = false;
            result |= (_value.i8 != 0);
            result |= (_value.i16 != 0);
            result |= (_value.i32 != 0);
            result |= (_value.i64 != 0);
            result |= (_value.ui8 != 0);
            result |= (_value.ui16 != 0);
            result |= (_value.ui32 != 0);
            result |= (_value.ui64 != 0);
            return result;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return false;
            break;
        }
        }
        return false;
    }

    Variant::operator int8_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (int8_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (int8_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (int8_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (int8_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (int8_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (int8_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (int8_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (int8_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (int8_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (int8_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator int16_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (int16_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (int16_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (int16_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (int16_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (int16_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (int16_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (int16_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (int16_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (int16_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (int16_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator int32_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (int32_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (int32_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (int32_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (int32_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (int32_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (int32_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (int32_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (int32_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (int32_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (int32_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator int64_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (int64_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (int64_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (int64_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (int64_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (int64_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (int64_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (int64_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (int64_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (int64_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (int64_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator uint8_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (uint8_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (uint8_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (uint8_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (uint8_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (uint8_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (uint8_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (uint8_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (uint8_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (uint8_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (uint8_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator uint16_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (uint16_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (uint16_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (uint16_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (uint16_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (uint16_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (uint16_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (uint16_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (uint16_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (uint16_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (uint16_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator uint32_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (uint32_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (uint32_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (uint32_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (uint32_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (uint32_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (uint32_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (uint32_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (uint32_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (uint32_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (uint32_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator uint64_t()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (uint64_t)_value.b;
            break;
        }
        case V_INT8:
        {
            return (uint64_t)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (uint64_t)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (uint64_t)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (uint64_t)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (uint64_t)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (uint64_t)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (uint64_t)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (uint64_t)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (uint64_t)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator double()
    {
        switch (_type) {
        case V_NULL:
        {
            return 0;
            break;
        }
        case V_BOOL:
        {
            return (double)_value.b;
            break;
        }
        case V_INT8:
        {
            return (double)_value.i8;
            break;
        }
        case V_INT16:
        {
            return (double)_value.i16;
            break;
        }
        case V_INT32:
        {
            return (double)_value.i32;
            break;
        }
        case V_INT64:
        {
            return (double)_value.i64;
            break;
        }
        case V_UINT8:
        {
            return (double)_value.ui8;
            break;
        }
        case V_UINT16:
        {
            return (double)_value.ui16;
            break;
        }
        case V_UINT32:
        {
            return (double)_value.ui32;
            break;
        }
        case V_UINT64:
        {
            return (double)_value.ui64;
            break;
        }
        case V_DOUBLE:
        {
            return (double)_value.d;
            break;
        }
        case V_STRING:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            return 0;
            break;
        }
        }
        return 0;
    }

    Variant::operator std::string()
    {
        std::stringstream ss;
        switch (_type) {
        case V_BOOL:
        {
            ss << (_value.b ? "true" : "false");
            break;
        }
        case V_INT8:
        case V_INT16:
        case V_INT32:
        case V_INT64:
        case V_UINT8:
        case V_UINT16:
        case V_UINT32:
        case V_UINT64:
        {
            ss << this->operator uint64_t();
            break;
        }
        case V_DOUBLE:
        {
            ss << this->operator double();
            break;
        }
        case V_STRING:
        {
            ss << *_value.s;
            break;
        }
        case V_NULL:
        case V_MAP:
        case V_ARRAY:
        default:
        {
            break;
        }
        }
        std::string str = ss.str();
        return str;
    }

    Variant::operator std::vector<Variant>()
    {
        if (_type == V_ARRAY)
            return *_value.l;
        return std::vector<Variant>();
    }

    Variant::operator std::map<std::string, Variant>() 
    {
        if (_type == V_MAP)
            return *_value.m;
        return std::map<std::string, Variant>();
    }

    
    bool Variant::operator==(VariantType type)
    {
        if (type == _V_NUMERIC)
            return _type == V_INT8 ||
            _type == V_INT8 ||
            _type == V_INT16 ||
            _type == V_INT32 ||
            _type == V_INT64 ||
            _type == V_UINT8 ||
            _type == V_UINT16 ||
            _type == V_UINT32 ||
            _type == V_UINT64 ||
            _type == V_DOUBLE;
        else
            return _type == type;
    }

    bool Variant::operator!=(VariantType type)
    {
        return !operator ==(type);
    }

    std::vector<Variant> Variant::toList()
    {
        if (_type == V_ARRAY)
            return *_value.l;
        return std::vector<Variant>();
    }

    std::map<std::string, Variant> Variant::toMap()
    {
        if (_type == V_MAP)
            return *_value.m;
        return std::map<std::string, Variant>();
    }

    int Variant::getType()
    {
        return _type;
    }

    bool Variant::isNull()
    {
        return _type == V_NULL;
    }

    bool Variant::fromJsonToVar(const std::string& json)
    {
        return parseJson(json);
    }

    std::string Variant::toString()
    {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);

        filljsonstr(writer, *this);

        std::string str = buffer.GetString();

        return std::move(str);
    }

    bool Variant::parseJson(const std::string& raw)
    {
        /*创建栈对象来保存每个map和vector*/
        typeStack = new std::stack<VariantType>;
        varStack = new std::stack<Variant*>;
        keyStack = new std::stack<std::string>;

        Reader reader;
        StringStream ss(raw.c_str());
        if (reader.Parse(ss, *this))
        {
            if (varStack)
            {
                delete varStack;
            }
            if (typeStack)
            {
                delete typeStack;
            }
            if (keyStack)
            {
                delete keyStack;
            }
            return true;
        }
        return false;
    }












	void Variant::internalCopy(const Variant& val)
	{
        _type = val._type;
        memset(&_value, 0, sizeof(_value));

        switch (val._type) {
        case V_STRING:
        {
            _value.s = new std::string(*val._value.s);
            break;
        }
        case V_MAP:
        {
            _value.m = new std::map<std::string, Variant>(*val._value.m);
            break;
        }
        case V_ARRAY:
        {
            _value.l = new std::vector<Variant>(*val._value.l);
            break;
        }
        default:
        {
            memcpy(&_value, &val._value, sizeof(_value));
            break;
        }
        }
	}

    void Variant::clear()
    {
        switch (_type)
        {
        case V_STRING:
        {
            delete _value.s;
            break;
        }
        case V_ARRAY:
        {
            delete _value.l;
            break;
        }
        case V_MAP:
        {
            delete _value.m;
            break;
        }
        default:
        {
            break;
        }
        }
        _type = V_NULL;
        memset(&_value, 0, sizeof(_value));
    }

 

    /*****************************************Hander***************************/
    bool Variant::Null()
    {
        //std::cout << "Null()" << std::endl; return true;
        fillmapjson("");
        return true;
    }
    bool Variant::Bool(bool b)
    {
        //std::cout << "Bool(" << boolalpha << b << ")" << std::endl; 
        fillmapjson(b);

        return true;
    }
    bool Variant::Int(int i)
    {
        //cout << "Int(" << i << ")" << endl; 
        fillmapjson(i);
        return true;
    }

    bool Variant::Uint(unsigned u)
    {
        //cout << "Uint(" << u << ")" << endl; 
        fillmapjson(u);
        return true;
    }
    bool Variant::Int64(int64_t i)
    {
        //cout << "Int64(" << i << ")" << endl; 
        fillmapjson(i);
        return true;
    }
    bool Variant::Uint64(uint64_t u)
    {
        //cout << "Uint64(" << u << ")" << endl; 
        fillmapjson(u);
        return true;
    }
    bool Variant::Double(double d)
    {
        //cout << "Double(" << d << ")" << endl; 
        fillmapjson(d);
        return true;
    }
    bool Variant::RawNumber(const char* str, unsigned length, bool copy)
    {
        //cout << "Number(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
        return true;
    }
    bool  Variant::String(const char* str, unsigned length, bool copy)
    {
        //cout << "String(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
        fillmapjson(str);
        return true;
    }
    bool Variant::Key(const char* str, unsigned length, bool copy)
    {
        //cout << "Key(" << str << ", " << length << ", " << boolalpha << copy << ")" << endl;
        std::string keystr = str;
        keyStack->push(keystr);

        return true;
    }
    bool Variant::StartObject()
    {
        //cout << "StartObject()" << endl;
        if (!_value.m && !_value.l)
        {
            /*解析 json 需要创建map*/
            _type = V_MAP;
            _value.m = new std::map<std::string, Variant>;

            /*入栈*/
            typeStack->push(_type);
            varStack->push(this);

            return true;
        }
        /*json中有 Object的话需要插入 map*/

        std::map<std::string, Variant> a;
        Variant* varmap = new Variant(a);

        VariantType type = V_MAP;

        typeStack->push(type);
        varStack->push(varmap);

        return true;
    }
    bool Variant::EndObject(unsigned memberCount)
    {
        //cout << "EndObject(" << memberCount << ")" << endl; 
        typeStack->pop();

        if (typeStack->empty())
        {
            return true;
        }

        Variant* varPtr = varStack->top();
        varStack->pop();

        Variant* varPtrFar = varStack->top();

        if (typeStack->top() == V_MAP)
        {
            std::map<std::string, Variant>* mapPtr = varPtrFar->_value.m;

            std::string str = keyStack->top();
            keyStack->pop();

            (*mapPtr)[str] = *varPtr;
        }
        else if (typeStack->top() == V_ARRAY)
        {
            std::vector<Variant>* vecPtr = varPtrFar->_value.l;

            vecPtr->push_back(*varPtr);
        }
        else
        {
            return false;
        }
        return true;
    }
    bool Variant::StartArray()
    {
        //cout << "StartArray()" << endl;
        if (!_value.m && !_value.l)
        {
            /*解析 json 需要创建vector   这个json不是json对象，是json数组*/
            _type = V_ARRAY;
            _value.l = new std::vector<Variant>;

            /*入栈*/
            typeStack->push(_type);
            varStack->push(this);

            return true;
        }

        std::vector<Variant> a;
        Variant* vec = new Variant(a);

        VariantType type = V_ARRAY;
        typeStack->push(type);
        varStack->push(vec);

        return true;
    }
    bool Variant::EndArray(unsigned elementCount)
    {
        //cout << "EndArray(" << elementCount << ")" << endl; 
        typeStack->pop();

        if (typeStack->empty())
        {
            return true;
        }

        //只有 对象下才能存数组，  数组下不能放数组
        if (typeStack->top() == V_MAP)
        {
            std::string str = keyStack->top();
            keyStack->pop();

            Variant* vecPtr = varStack->top();
            varStack->pop();

            Variant* ptr = varStack->top();

            std::map<std::string, Variant>* mapPtr = ptr->_value.m;

            (*mapPtr)[str] = *vecPtr;
        }

        return true;
    }

    void Variant::fillmapjson(Variant val)
    {
        Variant* p = varStack->top();

        if (typeStack->top() == V_MAP)
        {
            std::string str = keyStack->top();
            keyStack->pop();
            std::map<std::string, Variant>* objmap = p->_value.m;
            (*objmap)[str] = val;    // (*objmap)[keyval]  ==  Variant  ==》   Variant operator=(Variant &val)   
        }
        else if (typeStack->top() == V_ARRAY)
        {
            std::vector<Variant>* objvec = p->_value.l;
            objvec->push_back(val);
        }
    }

}

