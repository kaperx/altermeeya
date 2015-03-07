#include "profile.h"

Profile::Profile(const Profile::String &fileName)
    : fileName_(fileName)
{
}

Profile::String Profile::getString( const Profile::String &section, const Profile::String &keyName, const Profile::String &defVal ) const
{
    const int bufferSize = 2048;
    WCHAR retBuffer[bufferSize];
    DWORD dwResult = GetPrivateProfileStringW(section.c_str(), keyName.c_str(), defVal.c_str(), retBuffer, bufferSize, fileName_.c_str());
    return retBuffer;
}

Profile::String Profile::getString( const Profile::String &section, const Profile::String &keyName ) const
{
    const int bufferSize = 2048;
    WCHAR retBuffer[bufferSize];
    DWORD dwResult = GetPrivateProfileStringW(section.c_str(), keyName.c_str(), NULL, retBuffer, bufferSize, fileName_.c_str());
    return retBuffer;
}

bool Profile::writeString( const Profile::String &section, const Profile::String &keyName, const Profile::String &value )
{
    BOOL bResult = WritePrivateProfileStringW(section.c_str(), keyName.c_str(), value.c_str(), fileName_.c_str());
    return bResult != 0;
}

Profile::Section::Section( Profile &profile, const Profile::String &name )
    : profile_(profile)
    , name_(name)
{
}

Profile::String Profile::Section::getString( const Profile::String &keyName, const Profile::String &defVal ) const
{
    return profile_.getString(name_, keyName, defVal);
}

Profile::String Profile::Section::getString( const Profile::String &keyName ) const
{
    return profile_.getString(name_, keyName);
}

bool Profile::Section::writeString( const Profile::String &keyName, const Profile::String &value )
{
    return profile_.writeString(name_, keyName, value);
}
