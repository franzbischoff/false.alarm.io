#include <calculator.h>
#include <unity.h>

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}

void test_function_calculator_addition(void)
{
  TEST_ASSERT_EQUAL(32, addition(25, 7));
}

void test_function_calculator_subtraction(void)
{
  TEST_ASSERT_EQUAL(20, subtraction(23, 3));
}

void test_function_calculator_multiplication(void)
{
  TEST_ASSERT_EQUAL(50, multiplication(25, 2));
}

void test_function_calculator_division(void)
{
  TEST_ASSERT_EQUAL(32, division(100, 3));
}

void app_main()
{
  UNITY_BEGIN();

  RUN_TEST(test_function_calculator_addition);
  RUN_TEST(test_function_calculator_subtraction);
  RUN_TEST(test_function_calculator_multiplication);
  RUN_TEST(test_function_calculator_division);

  UNITY_END();
}
