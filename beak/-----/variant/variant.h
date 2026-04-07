#ifndef VARIANT_H_
#define VARIANT_H_


#include <string>
#include <vector>
#include <map>
#include <stack>



namespace CommandSpace
{
	typedef enum _VariantType {
		V_NULL = 1,
		V_BOOL = 3,
		V_INT8 = 4,
		V_INT16 = 5,
		V_INT32 = 6,
		V_INT64 = 7,
		V_UINT8 = 8,
		V_UINT16 = 9,
		V_UINT32 = 10,
		V_UINT64 = 11,
		V_DOUBLE = 12,
		_V_NUMERIC = 13,
		V_STRING = 17,
		V_BYTEARRAY = 18,
		V_MAP = 19,
		V_ARRAY = 20
	} VariantType;

	class Variant
	{
	public:
		Variant();
		Variant(VariantType type);
		Variant(const Variant& val);
		Variant(const bool& val);
		Variant(const int8_t& val);
		Variant(const int16_t& val);
		Variant(const int32_t& val);
		Variant(const int64_t& val);
		Variant(const uint8_t& val);
		Variant(const uint16_t& val);
		Variant(const uint32_t& val);
		Variant(const uint64_t& val);
		Variant(const double& val);
		Variant(const char* val);
		Variant(const std::string& val);
		Variant(const std::vector<Variant>& val);
		Variant(const std::map<std::string, Variant>& val);		
		~Variant();

		Variant& operator=(const Variant& val);
		Variant& operator=(const bool& val);
		Variant& operator=(const int8_t& val);
		Variant& operator=(const int16_t& val);
		Variant& operator=(const int32_t& val);
		Variant& operator=(const int64_t& val);
		Variant& operator=(const uint8_t& val);
		Variant& operator=(const uint16_t& val);
		Variant& operator=(const uint32_t& val);
		Variant& operator=(const uint64_t& val);
		Variant& operator=(const double& val);
		Variant& operator=(const char* val);
		Variant& operator=(const std::string& val);
		Variant& operator=(const std::vector<Variant>& val);
		Variant& operator=(const std::map<std::string, Variant>& val);
		

		operator VariantType();
		operator bool();
		operator int8_t();
		operator int16_t();
		operator int32_t();
		operator int64_t();
		operator uint8_t();
		operator uint16_t();
		operator uint32_t();
		operator uint64_t();
		operator double();
		operator std::string();
		operator std::vector<Variant>();	
		operator std::map<std::string, Variant>();


		bool operator==(VariantType type);
		bool operator!=(VariantType type);

	public:
		std::vector<Variant> toList();

		std::map<std::string, Variant> toMap();

		int getType();

		bool isNull();

	public: //map json string相关的操作 提供的转换函数:需要 Variant 变量为map类型的
		// json 格式的 传过来是 string -> Variant 类型的

		 bool fromJsonToVar(const std::string& json);

		 std::string toString();


	public:   /*Hander*/
		bool Null();
		bool Bool(bool b);
		bool Int(int i);
		bool Uint(unsigned u);
		bool Int64(int64_t i);
		bool Uint64(uint64_t u);
		bool Double(double d);
		bool RawNumber(const char* str, unsigned length, bool copy);
		bool String(const char* str, unsigned length, bool copy);
		bool Key(const char* str, unsigned length, bool copy);
		bool StartObject();
		bool EndObject(unsigned memberCount);
		bool StartArray();
		bool EndArray(unsigned elementCount);


	private:	//内部函数
		void internalCopy(const Variant& val);

		void clear();

		void fillmapjson(Variant val);

		bool parseJson(const std::string& raw);

	private:
		VariantType _type;
		union {
			bool b;
			int8_t i8;
			int16_t i16;
			int32_t i32;
			int64_t i64;
			uint8_t ui8;
			uint16_t ui16;
			uint32_t ui32;
			uint64_t ui64;
			double d;
			std::string* s;
			std::map<std::string, Variant>* m;
			std::vector<Variant>* l;
		} _value;

		std::stack<VariantType>* typeStack;
		std::stack<Variant*>* varStack;
		std::stack<std::string>* keyStack;
	};

	typedef std::map<std::string, Variant> VariantMap;
}

#endif //ALVA_VARIANT_H_


