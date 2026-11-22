#include <styleos/irq_route_807.h>
#include <stdint.h>
int irq807_select_pit(const struct acpi806_topology *t,
                      const uint32_t counts[ACPI806_MAX_IOAPIC],
                      struct irq807_plan *out){
 if(!t||!counts||!out||t->enabled_lapics==0||t->ioapic_count==0||t->ioapic_count>ACPI806_MAX_IOAPIC||t->iso_count>ACPI806_MAX_ISO)return 0;
 uint32_t gsi=0,flags=0;int found=0;
 for(uint32_t i=0;i<t->iso_count;i++){
  const struct acpi806_override *v=&t->iso[i];
  if(v->bus!=0||v->source>=16||v->flags&0xfff0u||((v->flags&3u)==2u)||(((v->flags>>2)&3u)==2u))return 0;
  if(v->source==0){if(found)return 0;found=1;gsi=v->gsi;flags=v->flags;}
 }
 /* For ISA, conform-to-bus means active high and edge triggered. Refuse
  * a low or level route until an IRQ polarity test is explicitly added. */
 uint32_t polarity=flags&3u,trigger=(flags>>2)&3u;
 if(polarity==3u||trigger==3u)return 0;
 int route=-1;uint32_t pin=0;
 for(uint32_t i=0;i<t->ioapic_count;i++){
  uint32_t base=t->ioapic[i].gsi_base,n=counts[i];
  if(n==0||n>120||base>UINT32_MAX-n)return 0;
  if(gsi>=base && gsi-base<n){if(route>=0)return 0;route=(int)i;pin=gsi-base;}
 }
 if(route<0)return 0;
 *out=(struct irq807_plan){0u,gsi,(uint32_t)route,pin,polarity,trigger};return 1;
}
