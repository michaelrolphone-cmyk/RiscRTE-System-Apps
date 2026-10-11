using SettingsCheckpoint=RiscPackages::SettingsCheckpoint;
alignas(SettingsCheckpoint) unsigned char settingsMemory[sizeof(SettingsCheckpoint)];SettingsCheckpoint*settingsCheckpoint;
unsigned settingsCursor;
/* Keep the original settings prefix usable when a newer consumer runs against
 * an older Runtime table. Never read the appended shared suffix before size. */
#ifndef RISC_INSTALLATION_SETTINGS_FORMAT_SIZE
#define RISC_INSTALLATION_SETTINGS_FORMAT_SIZE (offsetof(risc_installation_v1,settings_format)+sizeof(((risc_installation_v1*)0)->settings_format))
#endif
bool settingsApi(){return native&&native->struct_size>=offsetof(risc_installation_v1,settings_format)&&native->settings_capacity&&native->settings_status&&native->settings_restore&&native->settings_snapshot&&native->settings_ack;}
unsigned sharedSettingsCount(){
#ifdef RISC_INSTALLATION_SHARED_SETTINGS_SIZE
 if(settingsApi()&&native->struct_size>=RISC_INSTALLATION_SHARED_SETTINGS_SIZE&&native->settings_format==2&&native->shared_settings_namespace&&native->shared_settings_count<=NamespaceMax)
  return native->shared_settings_count;
#endif
 return 0;
}
uint32_t sharedSettingsNamespace(unsigned index){
#ifdef RISC_INSTALLATION_SHARED_SETTINGS_SIZE
 return native->shared_settings_namespace(native->context,index);
#else
 (void)index;return 0;
#endif
}
bool settingsReady(const Package&p){
 if(!p.settingsNeeded||!p.namespaceSlot||!settingsApi())return true;
 const auto status=native->settings_status(native->context,p.namespaceSlot);return status==1||status==2||status==4;
}
bool settingsPending(uint32_t slot){const auto status=native->settings_status(native->context,slot);return status!=1&&status!=4;}
bool settingsWanted(){
 if(!settingsApi())return false;
 for(unsigned i=0;i<sharedSettingsCount();++i)if(sharedSettingsNamespace(i)&&settingsPending(native->namespace_capacity+1+i))return true;
 for(unsigned i=0;i<currentCount;++i)if(current[i].settingsNeeded&&current[i].namespaceSlot&&settingsPending(current[i].namespaceSlot))return true;
 return false;
}
int32_t settingsStep(const risc_storage_volume_api_v1*v){
 if(!settingsCheckpoint){
  uint32_t slot=0;char sharedOwner[24]{};const char*owner=nullptr;
  /* Shared boot namespaces lead the initial pass. Advance even on failure so
   * one corrupt namespace cannot permanently starve the remaining namespaces. */
  const unsigned shared=sharedSettingsCount(),total=shared+currentCount;
  for(unsigned offset=0;offset<total;++offset){
   const unsigned i=(settingsCursor+offset)%total;
   if(i<shared){
    const uint32_t ns=sharedSettingsNamespace(i),candidate=native->namespace_capacity+1+i;
    if(!ns||!settingsPending(candidate))continue;
    std::snprintf(sharedOwner,sizeof(sharedOwner),"shared-%08x",unsigned(ns));slot=candidate;owner=sharedOwner;
   }else{
    const auto&p=current[i-shared];if(!p.settingsNeeded||!p.namespaceSlot||!settingsPending(p.namespaceSlot))continue;
    slot=p.namespaceSlot;owner=p.view.id;
   }
   settingsCursor=(i+1)%total;break;
  }
  if(!slot)return INSTALLED_APPS_OK;
  settingsCheckpoint=new(settingsMemory)SettingsCheckpoint();
  if(!settingsCheckpoint->start(native,v,slot,owner)){settingsCheckpoint->~SettingsCheckpoint();settingsCheckpoint=nullptr;installed_catalog_media(&catalog,false);return INSTALLED_APPS_UNAVAILABLE;}
  volume=v;
 }
 if(volume!=v)return INSTALLED_APPS_INVALID;
 settingsCheckpoint->step();const auto state=settingsCheckpoint->state();
 if(state==SettingsCheckpoint::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
 if(state!=SettingsCheckpoint::State::Done&&state!=SettingsCheckpoint::State::Failed&&state!=SettingsCheckpoint::State::Cancelled)return INSTALLED_APPS_WORKING;
 settingsCheckpoint->~SettingsCheckpoint();settingsCheckpoint=nullptr;volume=nullptr;
 if(state!=SettingsCheckpoint::State::Done)installed_catalog_media(&catalog,false);
 return state==SettingsCheckpoint::State::Done?INSTALLED_APPS_WORKING:INSTALLED_APPS_UNAVAILABLE;
}
int32_t settingsCancel(){
 if(!settingsCheckpoint)return INSTALLED_APPS_OK;
 settingsCheckpoint->cancel();settingsCheckpoint->step();auto state=settingsCheckpoint->state();
 if(state==SettingsCheckpoint::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
 if(state!=SettingsCheckpoint::State::Done&&state!=SettingsCheckpoint::State::Failed&&state!=SettingsCheckpoint::State::Cancelled)return INSTALLED_APPS_WORKING;
 settingsCheckpoint->~SettingsCheckpoint();settingsCheckpoint=nullptr;volume=nullptr;return INSTALLED_APPS_OK;
}
