#include <styleos/irq_route_807.h>
#include <assert.h>
#include <stdio.h>
int main(void){
 struct acpi806_topology t={0};struct irq807_plan p={0};
 t.enabled_lapics=1;t.ioapic_count=1;t.ioapic[0].physical=0xfec00000;t.ioapic[0].gsi_base=0;t.iso_count=1;
 t.iso[0]=(struct acpi806_override){0,0,2,0};
 uint32_t counts[ACPI806_MAX_IOAPIC]={24};
 assert(irq807_select_pit(&t,counts,&p) &&p.gsi==2&&p.pin==2&&p.trigger==0);
 unsigned cases=0;
 #define BAD() do{assert(!irq807_select_pit(&t,counts,&p));cases++;}while(0)
 counts[0]=0;BAD();counts[0]=24;
 t.iso[0].flags=0xf;BAD();t.iso[0].flags=0x2;BAD();t.iso[0].flags=0x8;BAD();t.iso[0].flags=0;
 t.ioapic[0].gsi_base=3;BAD();t.ioapic[0].gsi_base=0;
 t.iso_count=2;t.iso[1]=(struct acpi806_override){0,0,2,0};BAD();t.iso_count=1;
 t.iso[0].bus=1;BAD();t.iso[0].bus=0;
 counts[0]=255;BAD();counts[0]=24;
 t.ioapic_count=0;BAD();t.ioapic_count=1;
 t.enabled_lapics=0;BAD();t.enabled_lapics=1;
 t.ioapic_count=2;t.ioapic[1].gsi_base=2;counts[1]=24;BAD();
 t.ioapic_count=1;
 assert(irq807_select_pit(&t,counts,&p));
 printf("STYLEOS S807 host route PASS %u negatives\n",cases);return 0;
}
