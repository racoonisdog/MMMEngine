#include "UserScriptsGenerator.h"
#include "UserScriptMessageSignatures.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    namespace
    {
        struct MessageInfo
        {
            std::string messageName;  // BroadCast 시 사용할 이름
            std::string cppName;      // 함수 이름
            std::vector<std::string> paramTypes;
        };

        struct PropertyInfo
        {
            std::string name;
            std::string type;
        };

        struct ScriptInfo
        {
            std::string className;
            fs::path headerPath;  // Scripts 기준 상대 경로
            bool dontAutogen = false;
            std::vector<MessageInfo> messages;
            std::vector<PropertyInfo> properties;
        };

        // UTF-8 등으로 파일 읽기
        static std::string ReadFileAsUtf8(const fs::path& path)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f)
                return {};
            std::ostringstream os;
            os << f.rdbuf();
            return os.str();
        }

        // 타입 문자열 정규화 (const, &, * 제거 후 비교용)
        static std::string NormalizeType(std::string s)
        {
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                s.pop_back();
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
                s.erase(0, 1);
            size_t i = 0;
            while (i < s.size())
            {
                if (s.compare(i, 5, "const") == 0 && (i + 5 >= s.size() || !isalnum(static_cast<unsigned char>(s[i + 5]))))
                {
                    s.erase(i, 5);
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                        s.erase(i, 1);
                    continue;
                }
                if (s[i] == '&' || s[i] == '*')
                {
                    s.erase(i, 1);
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                        s.erase(i, 1);
                    continue;
                }
                ++i;
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                s.pop_back();
            return s;
        }

        // 파라미터 문자열 파싱: "int a, const CollisionInfo& e" -> ["int", "CollisionInfo"]
        static std::vector<std::string> ParseParameterTypes(const std::string& paramsStr)
        {
            std::vector<std::string> out;
            std::string s = paramsStr;
            size_t depth = 0;
            std::string current;
            for (size_t i = 0; i < s.size(); ++i)
            {
                char c = s[i];
                if (c == '<' || c == '(' || c == '[')
                    ++depth;
                else if (c == '>' || c == ')' || c == ']')
                    --depth;
                else if (c == ',' && depth == 0)
                {
                    std::string typePart = current;
                    size_t lastSpace = typePart.rfind(' ');
                    if (lastSpace != std::string::npos)
                        typePart = typePart.substr(lastSpace + 1);
                    else
                    {
                        while (!typePart.empty() && (typePart.back() == ' ' || typePart.back() == '\t'))
                            typePart.pop_back();
                    }
                    out.push_back(NormalizeType(typePart));
                    current.clear();
                    continue;
                }
                current += c;
            }
            if (!current.empty())
            {
                std::string typePart = current;
                size_t lastSpace = typePart.rfind(' ');
                if (lastSpace != std::string::npos)
                    typePart = typePart.substr(lastSpace + 1);
                else
                {
                    while (!typePart.empty() && (typePart.back() == ' ' || typePart.back() == '\t'))
                        typePart.pop_back();
                }
                out.push_back(NormalizeType(typePart));
            }
            return out;
        }

        // 시그니처 목록과 (name, paramTypes) 일치 여부
        static bool MatchesEngineSignature(
            const std::string& funcName,
            const std::vector<std::string>& paramTypes,
            const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs)
        {
            for (const auto& sig : engineSigs)
            {
                if (sig.messageName != funcName)
                    continue;
                if (sig.paramTypes.size() != paramTypes.size())
                    continue;
                bool match = true;
                for (size_t i = 0; i < sig.paramTypes.size(); ++i)
                {
                    if (NormalizeType(paramTypes[i]) != sig.paramTypes[i])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        }

        // 클래스 본문 추출 (class X : public ScriptBehaviour { ... } 에서 { ... } 부분).
        // outBodyEnd = 본문 끝('}' 다음) 위치.
        static std::string ExtractClassBody(const std::string& text, size_t classDeclEnd, size_t& outBodyEnd)
        {
            outBodyEnd = classDeclEnd;
            size_t i = classDeclEnd;
            while (i < text.size() && text[i] != '{')
                ++i;
            if (i >= text.size())
                return {};
            size_t start = i;
            int depth = 1;
            ++i;
            while (i < text.size() && depth > 0)
            {
                if (text[i] == '{')
                    ++depth;
                else if (text[i] == '}')
                    --depth;
                ++i;
            }
            if (depth != 0)
                return {};
            outBodyEnd = i;
            return text.substr(start, i - start);
        }

        // ScriptBehaviour 상속 클래스 이름 추출
        static bool FindScriptBehaviourClass(const std::string& text, std::string& outClassName, size_t& outDeclEnd)
        {
            // class [USERSCRIPTS ]Enemy : public ScriptBehaviour — 클래스명은 콜론 직전 마지막 단어
            std::regex re(R"re(class\s+(?:\w+\s+)*(\w+)\s*:\s*public\s+ScriptBehaviour)re");
            std::smatch m;
            if (!std::regex_search(text, m, re))
                return false;
            outClassName = m[1].str();
            outDeclEnd = static_cast<size_t>(m.position(0)) + m.length();
            return true;
        }

        static void ParseMessagesAndProperties(
            const std::string& classBody,
            const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs,
            ScriptInfo& info)
        {
            std::set<std::string> registeredMessages; // 중복 방지

            // 1) USCRIPT_MESSAGE() void Func(params);
            {
                std::regex re(R"re(USCRIPT_MESSAGE\s*\(\s*\)\s*void\s+(\w+)\s*\(([^)]*)\)\s*;)re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    std::string cppName = (*it)[1].str();
                    std::string params = (*it)[2].str();
                    if (registeredMessages.count(cppName))
                        continue;
                    registeredMessages.insert(cppName);
                    MessageInfo mi;
                    mi.messageName = cppName;
                    mi.cppName = cppName;
                    mi.paramTypes = ParseParameterTypes(params);
                    info.messages.push_back(std::move(mi));
                }
            }

            // 2) USCRIPT_MESSAGE_NAME("X") void Func(params);
            {
                std::regex re(R"re(USCRIPT_MESSAGE_NAME\s*\(\s*"([^"]*)"\s*\)\s*void\s+(\w+)\s*\(([^)]*)\)\s*;)re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    std::string messageName = (*it)[1].str();
                    std::string cppName = (*it)[2].str();
                    std::string params = (*it)[3].str();
                    if (registeredMessages.count(cppName))
                        continue;
                    registeredMessages.insert(cppName);
                    MessageInfo mi;
                    mi.messageName = messageName;
                    mi.cppName = cppName;
                    mi.paramTypes = ParseParameterTypes(params);
                    info.messages.push_back(std::move(mi));
                }
            }

            // 3) 매크로 없이 void Func(params); 선언만 있는 경우 -> 엔진 시그니처와 일치하면 등록
            {
                std::regex re(R"re(void\s+(\w+)\s*\(([^)]*)\)\s*;)re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    std::string cppName = (*it)[1].str();
                    std::string params = (*it)[2].str();
                    if (registeredMessages.count(cppName))
                        continue;
                    std::vector<std::string> paramTypes = ParseParameterTypes(params);
                    if (!MatchesEngineSignature(cppName, paramTypes, engineSigs))
                        continue;
                    registeredMessages.insert(cppName);
                    MessageInfo mi;
                    mi.messageName = cppName;
                    mi.cppName = cppName;
                    mi.paramTypes = std::move(paramTypes);
                    info.messages.push_back(std::move(mi));
                }
            }

            // 4) USCRIPT_PROPERTY() type name;
            {
                std::regex re(R"re(USCRIPT_PROPERTY\s*\(\s*\)\s+([^;=]+)\s+(\w+)\s*[;=])re");
                std::sregex_iterator it(classBody.begin(), classBody.end(), re);
                std::sregex_iterator end;
                for (; it != end; ++it)
                {
                    PropertyInfo pi;
                    pi.type = NormalizeType((*it)[1].str());
                    pi.name = (*it)[2].str();
                    info.properties.push_back(std::move(pi));
                }
            }
        }

        static std::vector<ScriptInfo> ParseHeaderFile(
            const fs::path& headerPath,
            const fs::path& scriptsDir,
            const std::vector<MMMEngine::UserScriptMessageSignature>& engineSigs)
        {
            std::vector<ScriptInfo> result;
            std::string text = ReadFileAsUtf8(headerPath);
            if (text.empty())
                return result;

            std::string className;
            size_t declEnd = 0;
            while (FindScriptBehaviourClass(text, className, declEnd))
            {
                ScriptInfo info;
                info.className = className;
                info.headerPath = fs::relative(headerPath, scriptsDir);
                size_t bodyEnd = 0;
                std::string body = ExtractClassBody(text, declEnd, bodyEnd);
                if (body.empty())
                    break;

                info.dontAutogen = (body.find("USCRIPT_DONT_AUTOGEN") != std::string::npos);
                if (!info.dontAutogen)
                    ParseMessagesAndProperties(body, engineSigs, info);

                result.push_back(std::move(info));

                // 다음 클래스 검색 (이 클래스 본문 이후부터)
                if (bodyEnd >= text.size())
                    break;
                text = text.substr(bodyEnd);
                declEnd = 0;
            }
            return result;
        }

        /// 짝이 되는 .cpp에 RTTR 등록이 이미 있으면 true (gen.cpp에서 스킵용)
        static bool HasOwnRttrRegistration(const fs::path& scriptsDir, const fs::path& headerPath, const std::string& className)
        {
            fs::path cppPath = scriptsDir / headerPath;
            cppPath.replace_extension(".cpp");
            if (!fs::exists(cppPath) || !fs::is_regular_file(cppPath))
                return false;
            std::string cppText = ReadFileAsUtf8(cppPath);
            if (cppText.find("RTTR_PLUGIN_REGISTRATION") == std::string::npos
                && cppText.find("RTTR_REGISTRATION") == std::string::npos)
                return false;
            if (cppText.find("class_<" + className + ">") == std::string::npos)
                return false;
            return true;
        }

        // gen.cpp 생성 (이미 RTTR 등록된 .cpp가 있는 클래스는 제외)
        static bool WriteGenCpp(const fs::path& scriptsDir, const std::vector<ScriptInfo>& scripts)
        {
            std::vector<const ScriptInfo*> toGen;
            for (const auto& s : scripts)
            {
                if (HasOwnRttrRegistration(scriptsDir, s.headerPath, s.className))
                    continue;
                toGen.push_back(&s);
            }

            fs::path genPath = scriptsDir / "UserScripts.gen.cpp";
            std::ofstream out(genPath, std::ios::binary);
            if (!out)
                return false;

            out << "// Auto-generated. Do not edit.\n";
            out << "#include \"Export.h\"\n";
            out << "#include \"ScriptBehaviour.h\"\n";
            out << "#include \"UserScriptsCommon.h\"\n";
            out << "#include \"Object.h\"\n";
            out << "#include \"rttr/registration\"\n";
            out << "#include \"rttr/detail/policies/ctor_policies.h\"\n\n";

            for (const auto* s : toGen)
            {
                // gen.cpp 가 Scripts/ 안에 있으므로, 같은 폴더 기준 상대 경로만 사용 (Scripts/ 접두어 없음)
                std::string incPath = s->headerPath.generic_string();
                std::replace(incPath.begin(), incPath.end(), '\\', '/');
                out << "#include \"" << incPath << "\"\n";
            }
            out << "\nusing namespace rttr;\nusing namespace MMMEngine;\n\nRTTR_PLUGIN_REGISTRATION\n{\n";

            for (const auto* s : toGen)
            {
                out << "\tregistration::class_<" << s->className << ">(\"" << s->className << "\")\n";
                out << "\t\t(rttr::metadata(\"wrapper_type_name\", \"ObjPtr<" << s->className << ">\"))";
                for (const auto& p : s->properties)
                    out << "\n\t\t.property(\"" << p.name << "\", &" << s->className << "::" << p.name << ")";
                out << ";\n\n";

                out << "\tregistration::class_<ObjPtr<" << s->className << ">>(\"ObjPtr<" << s->className << ">\")\n";
                out << "\t\t.constructor([]() { return Object::NewObject<" << s->className << ">(); })\n";
                out << "\t\t.method(\"Inject\", &ObjPtr<" << s->className << ">::Inject);\n\n";
            }
            out << "}\n";
            return true;
        }

        // 생성자 본문에서 REGISTER_BEHAVIOUR_MESSAGE 제외한 라인들 유지
        static std::vector<std::string> ExtractConstructorBodyWithoutRegisters(const std::string& ctorBody)
        {
            std::vector<std::string> lines;
            std::istringstream is(ctorBody);
            std::string line;
            while (std::getline(is, line))
            {
                std::string trimmed = line;
                while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n'))
                    trimmed.pop_back();
                size_t i = 0;
                while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t'))
                    ++i;
                trimmed = trimmed.substr(i);
                if (trimmed.empty())
                    continue;
                if (trimmed.find("REGISTER_BEHAVIOUR_MESSAGE") != std::string::npos)
                    continue;
                lines.push_back(trimmed);
            }
            return lines;
        }

        // 헤더에 생성자 주입: ClassName() { ... } 를 찾아 REGISTER_BEHAVIOUR_MESSAGE 목록으로 교체
        static std::string InjectConstructor(
            std::string headerText,
            const std::string& className,
            const std::vector<MessageInfo>& messages)
        {
            std::set<std::string> added;
            for (const auto& m : messages)
            {
                if (added.count(m.cppName))
                    continue;
                added.insert(m.cppName);
            }
            std::string registerList;
            for (const std::string& name : added)
                registerList += "        REGISTER_BEHAVIOUR_MESSAGE(" + name + ");\n";

            // ClassName() { ... } 패턴 찾기 (괄호 매칭)
            std::regex ctorRegex(std::string(R"re(\s*)re") + className + R"re(\s*\(\s*\)\s*\{)re");
            std::smatch m;
            if (!std::regex_search(headerText, m, ctorRegex))
                return headerText;

            size_t bodyStart = m.position(0) + m.length();
            int depth = 1;
            size_t i = bodyStart;
            while (i < headerText.size() && depth > 0)
            {
                if (headerText[i] == '{')
                    ++depth;
                else if (headerText[i] == '}')
                    --depth;
                ++i;
            }
            size_t bodyEnd = i - 1;
            std::string oldBody = headerText.substr(bodyStart, bodyEnd - bodyStart);
            std::vector<std::string> keepLines = ExtractConstructorBodyWithoutRegisters(oldBody);

            std::string newBody = "\n";
            for (const auto& l : keepLines)
                newBody += "        " + l + "\n";
            newBody += registerList;
            newBody += "\n        ";  // 닫는 } 를 새 줄 + 들여쓰기로 맞춤

            headerText.replace(bodyStart, bodyEnd - bodyStart, newBody);
            return headerText;
        }
    }

    bool UserScriptsGenerator::Generate(const fs::path& projectRootDir)
    {
        fs::path scriptsDir = projectRootDir / "Source" / "UserScripts" / "Scripts";
        if (!fs::exists(scriptsDir) || !fs::is_directory(scriptsDir))
            return true;

        std::vector<MMMEngine::UserScriptMessageSignature> engineSigs = MMMEngine::GetEngineMessageSignatures();
        std::vector<ScriptInfo> allScripts;

        std::error_code ecDir;
        auto dirIt = fs::recursive_directory_iterator(scriptsDir,
            fs::directory_options::skip_permission_denied, ecDir);
        if (ecDir)
            return true;
        for (const auto& e : dirIt)
        {
            if (!e.is_regular_file())
                continue;
            if (e.path().extension() != ".h")
                continue;
            std::vector<ScriptInfo> infos = ParseHeaderFile(e.path(), scriptsDir, engineSigs);
            for (ScriptInfo& info : infos)
            {
                info.headerPath = fs::relative(e.path(), scriptsDir);
                allScripts.push_back(std::move(info));
            }
        }

        // 스크립트 유무와 관계없이 gen.cpp 는 항상 생성 (빌드가 동일한 파일 집합을 사용하도록)
        if (!WriteGenCpp(scriptsDir, allScripts))
            return false;

        for (const ScriptInfo& info : allScripts)
        {
            if (info.dontAutogen || info.messages.empty())
                continue;
            fs::path headerPath = scriptsDir / info.headerPath;
            std::string text = ReadFileAsUtf8(headerPath);
            if (text.empty())
                continue;
            std::string newText = InjectConstructor(text, info.className, info.messages);
            if (newText == text)
                continue;

            fs::path backupPath = headerPath;
            backupPath += ".bak";
            std::error_code ec;
            if (fs::exists(backupPath))
                fs::remove(backupPath, ec);
            fs::rename(headerPath, backupPath, ec);

            std::ofstream out(headerPath, std::ios::binary);
            if (!out)
            {
                fs::rename(backupPath, headerPath, ec);
                continue;
            }
            out.write(newText.data(), static_cast<std::streamsize>(newText.size()));
            out.close();
            fs::remove(backupPath, ec);
        }

        return true;
    }
}
