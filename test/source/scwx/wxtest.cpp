#include <gtest/gtest.h>

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

int main(int argc, char** argv)
{
   boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                       boost::log::trivial::debug);

   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
