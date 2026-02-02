PWD := $(shell pwd)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

DRV_NAME := my_module_queue
MODULE_PATH := /lib/modules/$(shell uname -r)/extra

.PHONY: build run remove install uninstall clean check reload help 

all: build

build:
	@echo "Сборка модуля ядра..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

run:
	@echo "Загрузка модуля..."
	sudo insmod $(PWD)/$(DRV_NAME).ko
	@echo "Модуль загружен. Проверка:"
	@lsmod | grep $(DRV_NAME) || echo "Модуль не найден в списке загруженных"

remove:
	@echo "Выгрузка модуля..."
	sudo rmmod $(DRV_NAME).ko 2>/dev/null || sudo rmmod $(DRV_NAME) 2>/dev/null || true
	@echo "Модуль выгружен"

install:
	@echo "Установка модуля в систему..."
	sudo mkdir -p $(MODULE_PATH)
	sudo cp $(DRV_NAME).ko $(MODULE_PATH)/
	sudo depmod -a
	@echo "Модуль установлен"


uninstall:
	@echo "Удаление модуля из системы..."
	sudo modprobe -r $(DRV_NAME) 2>/dev/null || true
	sudo rm -f $(MODULE_PATH)/$(DRV_NAME).ko
	sudo depmod -a
	@echo "Модуль удален"

clean:
	@echo "Очистка файлов сборки..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod.o modules.order Module.symvers
	find . -name "*.o" -delete
	find . -name "*.cmd" -delete
	find . -name ".tmp_versions" -type d -exec rm -rf {} + 2>/dev/null || true

check:
	@echo "=== Проверка состояния модуля ==="
	@echo "1. В списке загруженных модулей:"
	@lsmod | grep -q $(DRV_NAME) && echo "✓ Модуль загружен" || echo "✗ Модуль не загружен"
	@echo ""
	@echo "2. В системных модулях:"
	@test -f $(MODULE_PATH)/$(DRV_NAME).ko && echo "✓ Модуль установлен" || echo "✗ Модуль не установлен"
	@echo ""
	@echo "3. Информация о модуле:"
	@modinfo $(DRV_NAME) 2>/dev/null || echo "Информация недоступна"

reload: remove build run
	@echo "Модуль перезагружен"

help:
	@echo "Доступные команды:"
	@echo "  make build     - сборка модуля"
	@echo "  make run       - загрузка модуля (insmod)"
	@echo "  make remove    - выгрузка модуля (rmmod)"
	@echo "  make install   - установка модуля в систему"
	@echo "  make uninstall - полное удаление модуля"
	@echo "  make clean     - очистка файлов сборки"
	@echo "  make check     - проверка состояния модуля"
	@echo "  make reload    - перезагрузка модуля"
	@echo "  make help      - эта справка"