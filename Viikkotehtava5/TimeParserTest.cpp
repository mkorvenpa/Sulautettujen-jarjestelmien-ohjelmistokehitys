#include <gtest/gtest.h>
#include "../TimeParser.h"

// Test suite: TimeParserTest
TEST(TimeParserTest, TestCaseCorrectTime) {
    // Test with correct time string
    char time_test[] = "012525";
    ASSERT_EQ(time_parse(time_test),5125);
}

TEST(TimeParserTest, TestCaseValueWrong) {
    // Test with wrong time values
    char time_test[] = "246060";
    ASSERT_EQ(time_parse(time_test),TIME_VALUE_ERROR);   
}

TEST(TimeParserTest, TestCaseValueCorrect) {
    // Test with correct time values
    char time_test[] = "235959";
    ASSERT_EQ(time_parse(time_test),86399);   
}

TEST(TimeParserTest, TestCaseArrayNull) {
    // Test with null time string
    char *time_test = NULL;
    ASSERT_EQ(time_parse(time_test),TIME_ARRAY_ERROR);   
}

TEST(TimeParserTest, TestCaseLenWrong) {
    // Test with wrong length time string
    char time_test[] = "14120";
    ASSERT_EQ(time_parse(time_test),TIME_LEN_ERROR);   
}

TEST(TimeParserTest, TestCaseLenCorrect) {
    // Test with correct length time string
    char time_test[] = "000120";
    ASSERT_EQ(time_parse(time_test),80);   
}





// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
