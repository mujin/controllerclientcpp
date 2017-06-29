#include "gtest/gtest.h"
#include "mujincontrollerclient/mujinjson.h"
#include <iostream> // for debug print

using namespace mujinjson;
static std::string s = "{\"a\":true,\"b\":1,\"c\":\"string\",\"d\":[null]}";
TEST(JsonTest, Parse) {
    rapidjson::Document d;
    EXPECT_NO_THROW({
        ParseJson(d, s);
    });
    std::string ts = s.substr(1);
    EXPECT_THROW( {
        ParseJson(d, ts);
    }, MujinJSONException);
}

TEST(JsonTest, Dump) {
    rapidjson::Document d;
    ParseJson(d, s);
    EXPECT_EQ(DumpJson(d), s);
}

TEST(JsonTest, GetInt) {
    rapidjson::Document d;
    d.Parse("{\"a\": 2147483647}"); //INT_MAX
    EXPECT_EQ(GetJsonValueByKey<int>(d, "a"), INT_MAX);
    EXPECT_EQ(GetJsonValueByKey<unsigned int>(d, "a"), INT_MAX);
    EXPECT_EQ(GetJsonValueByKey<unsigned long long>(d, "a"), INT_MAX);
    EXPECT_EQ(GetJsonValueByKey<uint64_t >(d, "a"), INT_MAX);
    //test case when key is missed
    EXPECT_EQ(GetJsonValueByKey<int>(d, "b", 0), 0);
    EXPECT_EQ(GetJsonValueByKey<unsigned int>(d, "b", 0), 0);
    EXPECT_EQ(GetJsonValueByKey<unsigned long long>(d, "b", 0), 0);
    EXPECT_EQ(GetJsonValueByKey<uint64_t >(d, "b", 0), 0);
    //test case when key is not missed but default value is provided
    EXPECT_EQ(GetJsonValueByKey<int>(d, "a", 0), INT_MAX);
    EXPECT_EQ(GetJsonValueByKey<unsigned int>(d, "a", 0), INT_MAX);
    EXPECT_EQ(GetJsonValueByKey<unsigned long long>(d, "a", 0), INT_MAX);
    EXPECT_EQ(GetJsonValueByKey<uint64_t >(d, "a", 0), INT_MAX);
    int x;
    LoadJsonValueByKey(d, "a", x);
    EXPECT_EQ(x, INT_MAX);
    x = 0;
    LoadJsonValueByKey(d, "b", x);
    EXPECT_EQ(x, 0);
    LoadJsonValueByKey(d, "a", x, -1);
    EXPECT_EQ(x, INT_MAX);
    LoadJsonValueByKey(d, "b", x, -1);
    EXPECT_EQ(x, -1);
}

TEST(JsonTest, GetDouble) {
    rapidjson::Document d;
    //case when integer is provided
    d.Parse("{\"a\":32}");
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "a"), 32);
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "a", 0), 32);
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "b", 0), 0);
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "b", 0), 0);
    //case when double is provided
    d.Parse("{\"a\":1.0111}");
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "a"), 1.0111);
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "a", 0), 1.0111);
    EXPECT_DOUBLE_EQ(GetJsonValueByKey<double>(d, "b", -1), -1);
    double x;
    LoadJsonValueByKey(d, "a", x);
    EXPECT_DOUBLE_EQ(x, 1.0111);
    x = 0;
    LoadJsonValueByKey(d, "b", x);
    EXPECT_DOUBLE_EQ(x, 0);
    LoadJsonValueByKey(d, "a", x, 1.0111);
    EXPECT_DOUBLE_EQ(x, 1.0111);
    LoadJsonValueByKey(d, "b", x, -1);
    EXPECT_DOUBLE_EQ(x, -1);
}

TEST(JsonTest, GetString) {
    rapidjson::Document d;
    d.Parse("{\"a\":\"{string}\"}");
    EXPECT_EQ(GetJsonValueByKey<std::string>(d, "a"), "{string}");
    EXPECT_EQ(GetJsonValueByKey<std::string>(d, "a", "{}"), "{string}");
    EXPECT_EQ(GetJsonValueByKey<std::string>(d, "b", "{}"), "{}");
    std::string x;
    LoadJsonValueByKey(d, "a", x);
    EXPECT_EQ(x, "{string}");
    x = "{}";
    LoadJsonValueByKey(d, "b", x);
    EXPECT_EQ(x, "{}");
    LoadJsonValueByKey(d, "a", x, "s");
    EXPECT_EQ(x, "{string}");
    LoadJsonValueByKey(d, "b", x, "s");
    EXPECT_EQ(x, "s");
}

TEST(JsonTest, GetBool) {
    rapidjson::Document d;
    d.Parse("{\"a\":true}");
    EXPECT_TRUE(GetJsonValueByKey<bool>(d, "a"));
    EXPECT_TRUE(GetJsonValueByKey<bool>(d, "a", false));
    EXPECT_FALSE(GetJsonValueByKey<bool>(d, "b", false));
    d.Parse("{\"a\":false}");
    EXPECT_FALSE(GetJsonValueByKey<bool>(d, "a"));
    EXPECT_FALSE(GetJsonValueByKey<bool>(d, "a", true));
    EXPECT_TRUE(GetJsonValueByKey<bool>(d, "b", true));
}

template<class T>
std::vector<T> Range(int N) { // generate python-range style list for test
    std::vector<T> v(N);
    for (int i = 0; i < N; i ++) {
        v[i] = i;
    }
    return v;
}

template<class U>
std::map<std::string, U> RangeMap(int N) { // generate  dummpy map for test
    std::map<std::string, U> p;
    for (int i = 0; i < N; i ++) {
        p[std::string(i, 'a')] = U(i);
    }
    return p;
}

TEST(JsonTest, GetVector) {
    rapidjson::Document d;
    d.Parse("{\"a\":[0, 1, 2]}");
    EXPECT_EQ(GetJsonValueByKey<std::vector<int> >(d, "a"), Range<int>(3));
    EXPECT_EQ(GetJsonValueByKey<std::vector<double> >(d, "a"), Range<double>(3));
    EXPECT_EQ(GetJsonValueByKey<std::vector<int> >(d, "a", Range<int>(0)), Range<int>(3));
    EXPECT_EQ(GetJsonValueByKey<std::vector<double> >(d, "a", Range<double>(0)), Range<double>(3));
    EXPECT_EQ(GetJsonValueByKey<std::vector<int> >(d, "b"), Range<int>(0));
    EXPECT_EQ(GetJsonValueByKey<std::vector<double> >(d, "b"), Range<double>(0));
    EXPECT_EQ(GetJsonValueByKey<std::vector<int> >(d, "b", Range<int>(10)), Range<int>(10));
    EXPECT_EQ(GetJsonValueByKey<std::vector<double> >(d, "b", Range<double>(10)), Range<double>(10));
}

TEST(JsonTest, GetMap) {
    rapidjson::Document d;
    d.Parse("{\"a\":{\"a\":\"b\", \"b\":\"a\"}}");
    std::map<std::string, std::string> p = GetJsonValueByKey<std::map<std::string, std::string> >(d, "a");
    EXPECT_EQ(p.size(),  2);
    EXPECT_EQ(p["a"], "b");
    EXPECT_EQ(p["b"], "a");
    EXPECT_TRUE((GetJsonValueByKey<std::map<std::string, std::string> >(d, "b").empty()));
}

TEST(JsonTest, GetArray) {
    rapidjson::Document d;
    d.Parse("{\"a\":[0, 1, 2]}");
    int a[3];
    LoadJsonValueByKey(d, "a", a);
    EXPECT_EQ(std::vector<int>(a, a + sizeof(a) / sizeof(int)), Range<int>(3));
}

template<class T>
void TestSaveJsonValue(const T& v) { // this involved many cases
    rapidjson::Document d;
    d.SetObject();
    SetJsonValueByKey(d, "a", v);
    std::string key = "a";
    SetJsonValueByKey(d, key, v); // set the key again
    EXPECT_EQ(GetJsonValueByKey<T>(d, "a"), v);
    EXPECT_EQ(GetJsonString(v), DumpJson(d["a"])); // check GetJsonString() works
}

TEST(JsonTest, SaveJsonValue) {
    TestSaveJsonValue<double>(0.0);
    TestSaveJsonValue<bool>(true);
    TestSaveJsonValue<bool>(false);
    TestSaveJsonValue<int>(INT_MAX);
    TestSaveJsonValue<int>(INT_MIN);
    TestSaveJsonValue<unsigned int>(UINT_MAX);
    TestSaveJsonValue<unsigned long long>(ULONG_LONG_MAX);
    TestSaveJsonValue<uint64_t>(ULONG_LONG_MAX);
    TestSaveJsonValue<std::string>("");
    TestSaveJsonValue<std::string>(std::string(10000, 'x'));
    TestSaveJsonValue<std::vector<int> >(Range<int>(1000));
    TestSaveJsonValue<std::vector<int> >(Range<int>(0));
    TestSaveJsonValue<std::vector<double> >(Range<double>(1000));
    TestSaveJsonValue<std::vector<double> >(Range<double>(0));
    TestSaveJsonValue<std::map<std::string, int> >(RangeMap<int>(100));
    TestSaveJsonValue<std::map<std::string, int> >(RangeMap<int>(0));
    TestSaveJsonValue<std::map<std::string, double> >(RangeMap<double>(100));
    TestSaveJsonValue<std::map<std::string, std::vector<int> > >(RangeMap<std::vector<int> >(100));
    TestSaveJsonValue<std::map<std::string, std::vector<int> > >(RangeMap<std::vector<int> >(100));
    TestSaveJsonValue<std::map<std::string, std::vector<std::string> > >(RangeMap<std::vector<std::string> >(100));
}

template<class U, class V>
void TestMismatchedType(const U& u) {
    rapidjson::Document d;
    d.SetObject();
    SetJsonValueByKey(d, "a", u);
    EXPECT_THROW({
        GetJsonValueByKey<V>(d, "a");
    }, MujinJSONException) << DumpJson(d);
}

TEST(JsonTest, MismatchedType) {
//    TestMismatchedType<int, std::string>(1);  // current behavior supposes type conversion is allowed
    TestMismatchedType<int, std::vector<int> >(1);
    //TestMismatchedType<int, ConnectionParameters>(1);
    TestMismatchedType<std::string, std::vector<double> >("");
//    TestMismatchedType<std::string, int>("");
    //TestMismatchedType<std::string, ConnectionParameters>("");
    TestMismatchedType<std::vector<int>, int>(Range<int>(1));
    TestMismatchedType<std::vector<int>, std::string>(Range<int>(1));
    //TestMismatchedType<std::vector<int>, ConnectionParameters>(Range<int>(1));
    //TestMismatchedType<ConnectionParameters, std::vector<int> >(ConnectionParameters());
    //TestMismatchedType<ConnectionParameters, int>(ConnectionParameters());
    //TestMismatchedType<ConnectionParameters, std::string>(ConnectionParameters());
    //TestMismatchedType<ConnectionParameters, bool>(ConnectionParameters());
    //TestMismatchedType<bool, ConnectionParameters>(true);
    //TestMismatchedType<bool, ConnectionParameters>(false);
}

template<class T>
void TestSharedPtr(const T& t, int N = 10) { //dumps and loads a list of shared pointer
    std::vector<boost::shared_ptr<T> > v;
    for (int i = 0; i < N; i ++) {
        v.push_back(boost::shared_ptr<T>(new T(t)));
    }
    rapidjson::Document d;
    d.SetObject();
    SetJsonValueByKey(d, "a", v);
    EXPECT_NO_THROW({
        std::vector<boost::shared_ptr<T> > nv = GetJsonValueByKey<std::vector<boost::shared_ptr<T> > >(d, "a");
        EXPECT_TRUE(int(nv.size()) == N);
    }) << DumpJson(d);
}

TEST(JsonTest, SharedPointer) {
    TestSharedPtr<int>(INT_MAX);
    TestSharedPtr<std::vector<int> >(Range<int>(10));
    TestSharedPtr<std::string>("hello");
    //TestSharedPtr<ConnectionParameters>(ConnectionParameters());
}

// void TestGetTransform(const std::string &s, bool valid=true) {
//     rapidjson::Document d;
//     ParseJson(d, s);
//     if (valid) {
//         EXPECT_NO_THROW({
//             GetTransform(d);
//         });
//     } else {
//         EXPECT_THROW({
//             GetTransform(d);
//         }, MujinJSONException);
//     }
// }

// TEST(JsonTest, GetTransform) {
//     TestGetTransform("{\"unit\":\"m\", \"translation_\":[0.0, 0.0, 0.0], \"quat_\":[1.0, 0.0, 0.0, 0.0]}");
//     TestGetTransform("{\"unit\":\"mm\", \"translation_\":[0.0, 0.0, 0.0], \"quat_\":[1.0, 0.0, 0.0, 0.0]}");
//     TestGetTransform("{\"unit\":\"mm\"}"); // when no field is applied, should handle correctly
//     TestGetTransform("{\"unit\":\"cm\", \"translation_\":[0.0, 0.0, 0.0], \"quat_\":[1.0, 0.0, 0.0, 0.0]}", false); //unsupported unit
// }

template<class T>
void TestParameterBase(const T& v = T()) {
    rapidjson::Document d;
    std::string jsonstr = v.GetJsonString();
    ParseJson(d, jsonstr);
    T u(d);
    std::cerr << jsonstr << std::endl;
    EXPECT_EQ(std::string(u.GetJsonString()), jsonstr);
}

template<class T>
T GetParameterBaseFromJsonString(const std::string& s) {
    rapidjson::Document d;
    ParseJson(d, s);
    return T(d);
}

// TEST(JsonTest, ParametersBase) {
//     //TestParameterBase(GetParameterBaseFromJsonString<ConnectionParameters>("{\"ip\":\"localhost\", \"port\":8999}"));
//     TestParameterBase(CameraParameters());
//     TestParameterBase(CameraParameters("cameraid"));
//     TestParameterBase(CalibrationData());
// //    TestParameterBase(DetectedObject()); cannot do it for DetectedObject as it cannot load from json
//     TestParameterBase(RegionParameters());
// }

// TEST(JsonTest, DetectedObject) {
//     DetectedObject detectedObject;
//     rapidjson::Document d;
//     EXPECT_NO_THROW({
//         std::string s = detectedObject.GetJsonString();
//         ParseJson(d, s);
//     });
// }

void TestValidateJson(const std::string &s, bool valid) {
    if (valid) {
        EXPECT_NO_THROW({
            ValidateJsonString(s);
        });
    } else {
        EXPECT_THROW({
            ValidateJsonString(s);
         }, MujinJSONException);
    }
}

TEST(JsonTest, Validate) {
    TestValidateJson("{}", true);
    TestValidateJson(s, true);
    TestValidateJson("{", false);
    TestValidateJson("fsdfs", false);
}
