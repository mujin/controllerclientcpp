#include <string>
#include <sstream>
#include <mujincontrollerclient/mujinjson.h>

// The tests will expect json documents containing these keys
using namespace mujinclient::mujinjson_external;
const std::string expectedJsonKey = "key1";

void TestExistingKey()
{
    rapidjson::Document document;
    document.SetObject();
    int expectedResult = 1;
    // Expected key and an extra key are in the document
    SetJsonValueByKey(document, expectedJsonKey.c_str(), expectedResult, document.GetAllocator());
    SetJsonValueByKey(document, "extraKey", expectedResult, document.GetAllocator());
    int result = -1;
    MUJINCLIENTJSON_LOAD_REQUIRED_JSON_VALUE_BY_KEY(document, expectedJsonKey.c_str(), result);
    if(result == expectedResult) {
        std::cout << "OK: For existing key test, we got result = " << result;
    }
    else {
        std::cout << "FAILED: For existing key test, we got result = " << result;
    }
    std::cout << " when expected result was " << expectedResult << "\n";
}

void TestNonExistentKey()
{
    rapidjson::Document document;
    document.SetObject();
    // Expected key is not in the document
    SetJsonValueByKey(document, "extraKey", 0, document.GetAllocator());
    int result = -1;
    int exceptionLine = -1;
    try
    {
        exceptionLine =  __LINE__ + 1;
        MUJINCLIENTJSON_LOAD_REQUIRED_JSON_VALUE_BY_KEY(document, expectedJsonKey.c_str(), result);
    }
    catch(const MujinJSONException& e)
    {
        std::stringstream ss;
        ss << "mujin (): [" << __FILE__<<", "<<  exceptionLine <<"] assert(mujinclient::mujinjson_external::LoadJsonValueByKey(document, key1, result))";
        if(ss.str() == std::string(e.what())) {
            std::cout << "OK: ";
        }
        else{
            std::cout << "FAILED: \n";
            std::cout << "Expected              = " << ss.str() << "\n";
        }
        std::cout << "Got exception message = " << e.what() << "\n";
    }
    catch(const std::exception& e)
    {
        std::cout << "FAILED: Caught an exception, but not the right type.\n" << "Exception message = " << e.what() << "\n";
    }
}

int main()
{
    TestExistingKey();
    TestNonExistentKey();
}
