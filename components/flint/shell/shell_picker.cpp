#include "shell/shell_picker.h"
#include "companion/miniapps.h"
#include "faces/FaceRegistry.h"
#include "render/builders.h"
#include "render/watch_display.h"
#include "i18n/i18n.h"
#include <cstring>
namespace {
int gIndex=0, lastCount=-1;
bool gDirty=true, confirmDelete=false;
const char* error=nullptr;
int count(){return 1+miniappsCount();}
const char* selectedId(){const auto* p=miniappAt(gIndex-1);return p?p->id:"digital";}
const char* selectedName(){const auto* p=miniappAt(gIndex-1);return p?p->name:"Spark Base";}
void move(int direction){gIndex=(gIndex+count()+direction)%count();confirmDelete=false;error=nullptr;gDirty=true;}
}
void shellPickerEnter(){gIndex=0;for(int i=0;i<miniappsCount();++i)if(!strcmp(miniappAt(i)->id,FaceRegistry::activeId()))gIndex=i+1;lastCount=count();confirmDelete=false;error=nullptr;gDirty=true;}
void shellPickerNavPrev(){move(-1);}
void shellPickerNavNext(){move(1);}
void shellPickerDelete(){
  if(gIndex==0){error="protected_package";gDirty=true;return;}
  confirmDelete=true;error=nullptr;gDirty=true;
}
bool shellPickerBack(){if(confirmDelete){confirmDelete=false;gDirty=true;return false;}return true;}
bool shellPickerConfirm(){
  if(confirmDelete){
    error=miniappDelete(selectedId());confirmDelete=false;gIndex=0;gDirty=true;return false;
  }
  error=miniappActivate(selectedId());gDirty=true;return error==nullptr;
}
bool shellPickerNeedsFull(){if(lastCount!=count()){gIndex=0;gDirty=true;}return gDirty;}
bool shellPickerNeedsRows(){return false;}
void shellPickerPaint(uint8_t fade){
  (void)fade;using namespace FlintRender;
  const bool ru=flintI18nLang()==FLINT_I18N_RU;
  auto scene=begin(ru?"Приложения":"Apps");
  if(confirmDelete){
    text(scene,140,88,ru?"Удалить пакет?":"Remove package?",White,Body,true);
    char name[64];fit(name,sizeof(name),selectedName(),224);
    text(scene,140,126,name,Accent,Body,true);
    text(scene,140,172,ru?"Возврат к Spark Base":"Returns to Spark Base",Muted,Body,true);
    footer(scene,ru?"Enter — удалить · Esc — назад":"Enter — remove · Esc — back");
  }else{
    counter(scene,gIndex,count());
    const int start=(gIndex/VisibleRows)*VisibleRows;
    for(int i=0;i<VisibleRows&&start+i<count();++i){
      const int n=start+i;const auto* p=miniappAt(n-1);
      const char* label=p?p->name:"Spark Base";
      const char* detail=!p?(ru?"Базовый · защищён":"Base · protected"):
          p->kind==1?(ru?"Тема":"Theme"):p->kind==0?(ru?"Циферблат":"Watch face"):(ru?"Комплект":"Bundle");
      char status[96];
      const bool active=p?(!strcmp(p->id,FaceRegistry::activeId())||!strcmp(p->id,miniappActiveTheme())):
          (!strcmp(FaceRegistry::activeId(),"digital")&&!*miniappActiveTheme());
      if(!p)snprintf(status,sizeof(status),"%s",ru?(active?"Выбран · защищён":"Защищён"):(active?"Active · protected":"Protected"));
      else snprintf(status,sizeof(status),"%s%s",active?(ru?"Выбран · ":"Active · "):"",detail);
      row(scene,i,label,status,n==gIndex);
    }
    footer(scene,error?(strcmp(error,"protected_package")? (ru?"Не удалось сохранить":"Could not save") : (ru?"Базовый пакет защищён":"Base package is protected")):
        (gIndex?"Enter · d — delete · Esc":"Enter · Esc"));
  }
  watchPresentScene(scene);lastCount=count();gDirty=false;
}
void shellPickerPaintPartial(){shellPickerPaint(255);}
