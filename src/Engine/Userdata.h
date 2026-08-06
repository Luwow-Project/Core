#pragma once

#include <lualib.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <new>

namespace Luwow::Engine {
    template <typename T>
    class Userdata {
    public:
        using MethodFn = int(T::*)(lua_State*);
        using GetterFn = int(T::*)(lua_State*);
        using SetterFn = int(T::*)(lua_State*);
    
        struct MethodDef {
            const char* name;
            MethodFn func;
        };
    
        struct PropertyDef {
            const char* name;
            GetterFn getter;
            SetterFn setter;
        };
    
        static void Register(lua_State* L, std::vector<MethodDef> methods, std::vector<PropertyDef> properties) {
            for (const auto& m : methods) GetMethods()[m.name] = m.func;
            for (const auto& p : properties) {
                GetGetters()[p.name] = p.getter;
                if (p.setter) GetSetters()[p.name] = p.setter;
            }
        
            luaL_newmetatable(L, T::Name);
        
            lua_pushcfunction(L, NameCall, T::Name);
            lua_setfield(L, -2, "__namecall");
        
            lua_pushcfunction(L, Index, T::Name);
            lua_setfield(L, -2, "__index");
        
            lua_pushcfunction(L, NewIndex, T::Name);
            lua_setfield(L, -2, "__newindex");
        
            lua_pushstring(L, T::Name);
            lua_setfield(L, -2, "__type");
        
            lua_pop(L, 1);
        }
    
        template <typename... Args>
        static T* Push(lua_State* L, Args&&... args) {
            void* memory = lua_newuserdatadtor(L, sizeof(T), Destructor);
            T* instance = new (memory) T(std::forward<Args>(args)...); // Placement new to construct the C++ object in Luau's memory block.
        
            luaL_getmetatable(L, T::Name);
            lua_setmetatable(L, -2);
        
            return instance;
        }
    
        static T* Check(lua_State* L, int index) {
            return static_cast<T*>(luaL_checkudata(L, index, T::Name));
        }
    
    private:
        static void Destructor(void* ptr) {
            static_cast<T*>(ptr)->~T();
        }
    
        static int NameCall(lua_State* L) {
            T* self = Check(L, 1);
            const char* method = lua_namecallatom(L, nullptr);
            
            auto& methods = GetMethods();
            auto it = methods.find(method);
            if (it != methods.end()) {
                return (self->*(it->second))(L);
            }
        
            luaL_error(L, "Attempt to call missing method '%s' on %s", method, T::Name);
            return 0;
        }
    
        static int Index(lua_State* L) {
            T* self = Check(L, 1);
            const char* key = luaL_checkstring(L, 2);
        
            auto& getters = GetGetters();
            auto propIt = getters.find(key);
            if (propIt != getters.end()) {
                return (self->*(propIt->second))(L);
            }
        
            // Support for obj.method instead of obj:method()
            auto& methods = GetMethods();
            if (methods.find(key) != methods.end()) {
                lua_pushstring(L, key);
                lua_pushcclosure(L, MethodClosure, key, 1);
                return 1;
            }
        
            luaL_error(L, "%s has no property or method '%s'", T::Name, key);
            return 0;
        }
    
        static int NewIndex(lua_State* L) {
            T* self = Check(L, 1);
            const char* key = luaL_checkstring(L, 2);
        
            auto& setters = GetSetters();
            auto it = setters.find(key);
            if (it != setters.end()) {
                return (self->*(it->second))(L);
            }
            
            luaL_error(L, "Property '%s' of %s is read-only or doesn't exist", key, T::Name);
            return 0;
        }
    
        static int MethodClosure(lua_State* L) {
            T* self = Check(L, 1);
            const char* method = lua_tostring(L, lua_upvalueindex(1));
            return (self->*(GetMethods()[method]))(L);
        }
    
        static std::unordered_map<std::string, MethodFn>& GetMethods() {
            static std::unordered_map<std::string, MethodFn> methods;
            return methods;
        }
        static std::unordered_map<std::string, GetterFn>& GetGetters() {
            static std::unordered_map<std::string, GetterFn> getters;
            return getters;
        }
        static std::unordered_map<std::string, SetterFn>& GetSetters() {
            static std::unordered_map<std::string, SetterFn> setters;
            return setters;
        }
    };
} // namespace Luwow::Engine