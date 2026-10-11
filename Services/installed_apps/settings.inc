using SettingsCheckpoint=RiscPackages::InstalledSettingsCheckpoint;
alignas(SettingsCheckpoint) unsigned char settingsMemory[sizeof(SettingsCheckpoint)];SettingsCheckpoint*settingsCheckpoint;
bool settingsApi(){return native&&native->struct_size>=sizeof(*native)&&native->settings_capacity&&native->settings_status&&native->settings_restore&&native->settings_snapshot&&native->settings_ack;}
bool settingsReady(const Package&p){
 if(!p.settingsNeeded||!p.namespaceSlot||!settingsApi())return true;
 const auto status=native->settings_status(native->context,p.namespaceSlot);return status==1||status==2;
}
bool settingsWanted(){
 if(!settingsApi())return false;
 for(unsigned i=0;i<currentCount;++i)if(current[i].settingsNeeded&&current[i].namespaceSlot&&native->settings_status(native->context,current[i].namespaceSlot)!=1)return true;
 return false;
}
int32_t settingsStep(const risc_storage_volume_api_v1*v){
 if(!settingsCheckpoint){
  for(unsigned i=0;i<currentCount;++i){const auto&p=current[i];if(!p.settingsNeeded||!p.namespaceSlot||native->settings_status(native->context,p.namespaceSlot)==1)continue;
   settingsCheckpoint=new(settingsMemory)SettingsCheckpoint();
   if(!settingsCheckpoint->start(native,v,p.namespaceSlot,p.view.id)){settingsCheckpoint->~SettingsCheckpoint();settingsCheckpoint=nullptr;installed_catalog_media(&catalog,false);return INSTALLED_APPS_UNAVAILABLE;}
   volume=v;break;
  }
  if(!settingsCheckpoint)return INSTALLED_APPS_OK;
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
