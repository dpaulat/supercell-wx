#pragma once

#define SCWX_SETTINGS_ENUM_VALIDATOR(Type, Iterator, ToName)                   \
   [](const std::string& value)                                                \
   {                                                                           \
      for (Type enumValue : Iterator)                                          \
      {                                                                        \
         /* If the value is equal to a lower case name */                      \
         std::string enumName = ToName(enumValue);                             \
         boost::to_lower(enumName);                                            \
         if (value == enumName)                                                \
         {                                                                     \
            /* Regard as a match, valid */                                     \
            return true;                                                       \
         }                                                                     \
      }                                                                        \
                                                                               \
      /* No match found, invalid */                                            \
      return false;                                                            \
   }
