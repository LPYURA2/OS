# StyleOS — контроль восстановления 730/744 (этап 758)

Read-only проверка контрольных бинарных образов и native app-политики `.tea`. **Не новая сборка операционной системы.**

```
python3 tools/check_baseline_730.py \
  --730 /path/to/StyleOS-730-Test.zip \
  --744 /path/to/StyleOS-744-Documents-Intermediate.zip \
  --catalog /path/to/installer/system-apps.json
python3 -m unittest discover -s tests -p 'test_baseline_730.py' -v
```

Сценарии не изменяют образы. Если источник отсутствует, тест помечается не выполненным. Отдельно см. `docs/RECOVERY_DECISION_758_RU.md`, `docs/CONTINUE.md`, `docs/DOCUMENTS_14_SCREENS.md`.

Полные рабочие исходники самих «Документов» (пока неполные относительно 14 макетов) сохранены на ветке `feature/styleos-system-app-preinstall-757` и в предыдущих архивах.
