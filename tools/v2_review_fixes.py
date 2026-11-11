from pathlib import Path
p=Path('src/dock_actions.cpp');s=p.read_text(encoding='utf-8-sig');needle='    QFile existing(configFile("presets.json"));';addition='''    // Presets are reusable layouts: encode light relationships by layer slot, never foreign UUID.
    auto encodeLight=[&](QJsonObject &light){for(const auto&key:{QString("light_bind"),QString("receiver_uuid")}){const QString id=light.value(key).toString();int index=-1;for(size_t i=0;i<model.layers.size();++i)if(QString::fromStdString(model.layers[i].sourceUuid)==id){index=int(i);break;}light.insert(key+"_layer",index);light.remove(key);}};
    encodeLight(settings);auto extra=settings.value("extra_lights").toArray();for(qsizetype i=0;i<extra.size();++i){auto light=extra[i].toObject();encodeLight(light);extra[i]=light;}settings.insert("extra_lights",extra);
''';s=s.replace(needle,addition+needle)
old='current.hasDefault=identity.hasDefault;}}}'
new='''current.hasDefault=identity.hasDefault;}}
        auto remap=[&](Light&light,const QJsonObject&data){auto field=[&](const char*key,std::string&uuid){const QString slot=QString(key)+"_layer";if(data.contains(slot)){const int i=data.value(slot).toInt(-1);uuid=i>=0&&size_t(i)<model.layers.size()?model.layers[size_t(i)].sourceUuid:std::string();}else if(!uuid.empty()&&std::none_of(model.layers.begin(),model.layers.end(),[&](const Layer&l){return l.sourceUuid==uuid;}))uuid.clear();};field("light_bind",light.bindUuid);field("receiver_uuid",light.receiverUuid);};
        remap(model.light,settings);const auto extra=settings.value("extra_lights").toArray();for(size_t i=0;i<model.extraLights.size()&&i<size_t(extra.size());++i)remap(model.extraLights[i],extra[int(i)].toObject());}
''';assert old in s;s=s.replace(old,new);p.write_text(s,encoding='utf-8')
p=Path('src/stage.cpp');s=p.read_text(encoding='utf-8-sig').replace('const auto&caster=m.layers[c];','const auto&caster=drawLayers[c];');p.write_text(s,encoding='utf-8')
p=Path('installer/setup.cpp');s=p.read_text(encoding='utf-8-sig');start=s.index('  HKEY key=nullptr;if(RegCreateKeyExW',s.index('int install('));end=s.index('  auto links=',start);regblock=s[start:end]
s=s[:start]+s[end:];s=s.replace('std::vector<fs::path>changed;','std::vector<fs::path>changed;std::vector<std::pair<fs::path,fs::path>> changedLinks;')
s=s.replace('  auto links=known(FOLDERID_CommonPrograms)/L"OBS Spatial FX";unsigned portable=0;', '''  auto installLink=[&](const fs::path&file,const fs::path&exe,const std::wstring&args,const fs::path&cwd){fs::path backup;if(fs::exists(file)){backup=staging/(L"link-"+std::to_wstring(changedLinks.size())+L".backup");fs::copy_file(file,backup);}changedLinks.push_back({file,backup});shortcut(file,exe,args,cwd);};
  auto links=known(FOLDERID_CommonPrograms)/L"OBS Spatial FX";unsigned portable=0;''')
s=s.replace('++portable;shortcut(links/','++portable;installLink(links/').replace('  shortcut(links/L"Uninstall OBS Spatial FX.lnk"','  installLink(links/L"Uninstall OBS Spatial FX.lnk"')
s=s.replace('base);committed=true;', '''base);
'''+regblock+'''  committed=true;''')
s=s.replace(' }catch(...){for(auto it=changed.rbegin();',''' }catch(...){bool restored=true;for(auto it=changedLinks.rbegin();it!=changedLinks.rend();++it){std::error_code ec;if(!it->second.empty())fs::copy_file(it->second,it->first,fs::copy_options::overwrite_existing,ec);else fs::remove(it->first,ec);if(ec)restored=false;}
 for(auto it=changed.rbegin();''');s=s.replace('else fs::remove(base/(*it),ec);}std::error_code ec;fs::remove_all(staging,ec);throw;', 'else fs::remove(base/(*it),ec);if(ec)restored=false;}if(restored){std::error_code ec;fs::remove_all(staging,ec);}else log(L"BLOCKED rollback incomplete; recovery files preserved: "+staging.wstring());throw;')
# Registry values are written as the last commit step. Restore exact old value types/data if any write fails.
needle='void shortcut(const fs::path&file'
helper='''struct RegistryValue{std::wstring name;DWORD type=0;std::vector<BYTE>data;};
std::vector<RegistryValue> readRegistry(HKEY key){std::vector<RegistryValue> values;for(DWORD i=0;;++i){wchar_t name[16384];DWORD chars=16384,size=0,type=0;LONG status=RegEnumValueW(key,i,name,&chars,nullptr,&type,nullptr,&size);if(status==ERROR_NO_MORE_ITEMS)break;if(status!=ERROR_SUCCESS)throw std::runtime_error("registry snapshot failed");RegistryValue v;v.name.assign(name,chars);v.type=type;v.data.resize(size);if(RegQueryValueExW(key,v.name.c_str(),nullptr,nullptr,v.data.data(),&size)!=ERROR_SUCCESS)throw std::runtime_error("registry snapshot data failed");values.push_back(std::move(v));}return values;}
void registerInstall(const fs::path&base){
 HKEY key=nullptr;DWORD disposition=0;if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,uninstallKey,0,nullptr,0,KEY_READ|KEY_WRITE|KEY_WOW64_64KEY,nullptr,&key,&disposition)!=ERROR_SUCCESS)throw std::runtime_error("uninstall registration requires administrator");
 std::vector<RegistryValue> before;try{before=readRegistry(key);regSet(key,L"DisplayName",L"OBS Spatial FX");regSet(key,L"DisplayVersion",L"2.0.0");regSet(key,L"Publisher",L"OBS Spatial FX contributors");regSet(key,L"InstallLocation",base.wstring());regSet(key,L"UninstallString",quote(base/L"Uninstall.exe")+L" /UNINSTALL");regSet(key,L"QuietUninstallString",quote(base/L"Uninstall.exe")+L" /UNINSTALL /SILENT");DWORD one=1;for(auto*name:{L"NoModify",L"NoRepair"})if(RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<BYTE*>(&one),4)!=ERROR_SUCCESS)throw std::runtime_error("registry flags write failed");}
 catch(...){if(disposition==REG_CREATED_NEW_KEY){RegCloseKey(key);RegDeleteTreeW(HKEY_LOCAL_MACHINE,uninstallKey);}else{const auto current=readRegistry(key);for(const auto&v:current)RegDeleteValueW(key,v.name.c_str());bool ok=true;for(const auto&v:before)if(RegSetValueExW(key,v.name.c_str(),0,v.type,v.data.data(),DWORD(v.data.size()))!=ERROR_SUCCESS)ok=false;RegCloseKey(key);if(!ok)log(L"BLOCKED registry rollback failed");}throw;}RegCloseKey(key);
}
''';s=s.replace(needle,helper+needle)
start=s.index('  HKEY key=nullptr;if(RegCreateKeyExW',s.index('int install('));end=s.index('  committed=true;',start);s=s[:start]+'  registerInstall(base);\n'+s[end:]
p.write_text(s,encoding='utf-8')
