# StyleOS — продолжение после 777

Репозиторий: `LPYURA2/OS`.
База 776: `379e9c52569463aa763d79d35b3e5a30fdc5240a`, CI 44954257459 PASS.
Ветка 777: `feature/styleos-attested-detached-roots-777`.
Сверь точный финальный коммит 777 и его CI; без успешного QEMU не принимать.

Изучи `AGENTS.md`, `docs/PROJECT.md`,
`docs/architecture/ATTESTED_DETACHED_ROOTS_777.md`,
`docs/architecture/DESKTOP_FRAME_GRANTS_765.md` и отчёты 768–776.

В 777 добавлен аппаратно допущенный мост между страницами тестового
PE-образа и неактивными раздельными page roots SAFE/user A/user B.
Допуск проходит только после политики UEFI/старого CR3 walker и
проверки W^X/изоляции. **Полного kernel mapping, PMM и независимого
графического .tea в Desktop ещё нет. Не переключать CR3 на эти корни.**

Этап 778: доказать полный SAFE mapping (PE code/data, kernel stacks,
GDT/IDT/TSS, trap handlers, framebuffer/MMIO, драйверы); затем
отдельный QEMU аппаратный switch/restore. Безопасное подключение
nts_prepare → ndl_admit к desktop scheduler — следующий шаг.

Установщик приложений .tea не завершён: есть host
`preinstall_tea.py` и окно установки, но нет гостевой установки в StyleFS,
ярлыков, удаления и обновлений. После независимых приложений закончить
его, затем экран входа с выбором пользователя по макету, защищённые
учётные записи, права StyleFS и финальный Проводник.

Каждый этап в новой ветке, main/принятые сборки не менять. Всегда
исходники ZIP точного CI, ISO, журналы и точка продолжения.
