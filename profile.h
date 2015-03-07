#ifndef ALTERMEEYA_PROFILE_H
#define ALTERMEEYA_PROFILE_H

#include <windows.h>
#include <string>

class Profile
{
public:
    typedef std::wstring String;
    Profile(const String &fileName);

    String getString(const String &section, const String &keyName, const String &defVal) const;
    String getString(const String &section, const String &keyName) const;
    bool writeString(const String &section, const String &keyName, const String &value);

    class Section
    {
        Profile &profile_;
        String name_;
    public:
        Section(Profile &profile, const String &name);

        String getString(const String &keyName, const String &defVal) const;
        String getString(const String &keyName) const;
        bool writeString(const String &keyName, const String &value);
    };
private:
    String fileName_;
};

#endif /* ALTERMEEYA_PROFILE_H */
