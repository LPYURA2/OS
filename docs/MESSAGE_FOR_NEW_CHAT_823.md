Продолжаем StyleOS, репозиторий `LPYURA2/OS`.

Этап 822 принят: ветка feature/styleos-acpica-namespace-init-822,
SHA c5e7a3c36b6fc1129851edc50f6e1d0eddd71f3f,
CI https://github.com/LPYURA2/OS/actions/runs/45270768614 SUCCESS.

Текущий эксперимент 823: feature/styleos-acpica-real-hostbridge-823.
Проверь финальный SHA и полный успешный CI, журналы QEMU/OVMF, точный архив
исходников и SHA-256. Этап не принимать по локальным тестам.

S823 переносит только проверенные копии оригинальных DSDT/SSDT прошивки в
ограниченный гостевой ACPICA после EBS. Проверять реальное выполнение
PCI0._HID и PCI0._CRS; _SEG/_BBN могут отсутствовать. Ни эти результаты,
ни resource buffer не дают разрешений ECAM/DMA. Только QEMU, не физическое ПК.

Затем ресурсы hostbridge/ECAM, IOMMU, аппаратные DMA, UEFI runtime, CR3,
общая приёмка обычного ядра с Desktop, процессами, .tea, StyleFS.
После фундамента по неизменному плану — процессы и ФС, отделение приложений
от прошивки, тесты VirtualBox, затем дальнейшая разработка ОС.

Не менять main, принятые ветки, UI, макеты. ФУНДАМЕНТ НЕ ЗАВЕРШЁН.
