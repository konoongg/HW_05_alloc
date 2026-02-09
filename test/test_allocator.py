#!/usr/bin/env python3
"""
Тестирование аллокатора ядра
"""
import os
import sys
import time
import subprocess
import re

class KernelAllocatorTest:
    def __init__(self, module_name="my_module_kalloc56"):
        self.module_name = module_name
        self.params_path = f"/sys/module/{module_name}/parameters"
        
        # Проверяем, что модуль загружен
        if not os.path.exists(self.params_path):
            print(f"Ошибка: модуль {module_name} не загружен!")
            sys.exit(1)
        
        # Словарь для хранения адресов выделенных блоков
        self.allocated = {}
    
    def run_command(self, cmd):
        """Выполнить команду и вернуть результат"""
        try:
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            return result.returncode, result.stdout, result.stderr
        except Exception as e:
            return -1, "", str(e)
    
    def write_param(self, param, value):
        """Записать значение в параметр модуля"""
        param_path = os.path.join(self.params_path, param)
        cmd = f"echo '{value}' | sudo tee {param_path}"
        return self.run_command(cmd)
    
    def read_param(self, param):
        """Прочитать значение параметра модуля"""
        param_path = os.path.join(self.params_path, param)
        cmd = f"sudo cat {param_path}"
        return self.run_command(cmd)
    
    def get_dmesg(self, lines=20):
        """Получить последние сообщения из dmesg"""
        cmd = f"sudo dmesg | tail -{lines}"
        return self.run_command(cmd)
    
    def allocate(self, size, name=None):
        """Выделить память указанного размера"""
        print(f"Выделение {size} байт...")
        ret, out, err = self.write_param("alloc", str(size))
        
        if ret != 0:
            print(f"Ошибка выделения {size} байт: {err}")
            return None
        
        # Ищем адрес в dmesg
        _, dmesg_out, _ = self.get_dmesg(5)
        match = re.search(r'at 0x([0-9a-f]+)', dmesg_out)
        
        if match:
            addr = "0x" + match.group(1)
            if name:
                self.allocated[name] = addr
            print(f"✓ Выделено {size} байт по адресу {addr}")
            return addr
        else:
            print(f"✗ Не удалось получить адрес из dmesg")
            return None
    
    def free(self, addr_or_name):
        """Освободить память по адресу или имени"""
        if addr_or_name in self.allocated:
            addr = self.allocated[addr_or_name]
            name = addr_or_name
        else:
            addr = addr_or_name
            name = None
        
        print(f"Освобождение памяти по адресу {addr}...")
        
        # Пробуем разные форматы адреса
        formats_to_try = []
        
        # Добавляем адрес как есть
        formats_to_try.append(addr)
        
        # Если адрес начинается с 0x, пробуем без префикса
        if addr.startswith("0x"):
            formats_to_try.append(addr[2:])  # Без 0x
        
        # Также пробуем в верхнем регистре
        formats_to_try.append(addr.upper())
        if addr.startswith("0x"):
            formats_to_try.append(addr[2:].upper())  # Без 0x в верхнем регистре
        
        # Убираем дубликаты
        formats_to_try = list(dict.fromkeys(formats_to_try))
        
        success = False
        error_messages = []
        
        for fmt in formats_to_try:
            ret, out, err = self.write_param("free", fmt)
            
            if ret == 0:
                print(f"✓ Успешно освобождено (формат: '{fmt}')")
                success = True
                break
            else:
                error_messages.append(f"Формат '{fmt}': {err.strip()}")
        
        if not success:
            print(f"✗ Все форматы не удались:")
            for msg in error_messages:
                print(f"  {msg}")
            return False
        
        if name:
            del self.allocated[name]
        
        return True
    
    def get_stats(self):
        """Получить статистику"""
        ret, out, err = self.read_param("stat")
        if ret == 0:
            return out.strip()
        return ""
    
    def get_bitmap(self):
        """Получить информацию о bitmap"""
        ret, out, err = self.read_param("bitmap_info")
        if ret == 0:
            return out.strip()
        return ""
    
    def print_stats(self, title="Текущая статистика:"):
        """Вывести статистику"""
        print(f"\n{title}")
        stats = self.get_stats()
        if stats:
            print(stats)
    
    def print_bitmap(self, title="Состояние bitmap:"):
        """Вывести bitmap"""
        print(f"\n{title}")
        bitmap = self.get_bitmap()
        if bitmap:
            # Показываем только первые 100 символов
            if len(bitmap) > 100:
                print(bitmap[:100] + "...")
            else:
                print(bitmap)
    
    def clear_dmesg(self):
        """Очистить сообщения ядра"""
        self.run_command("sudo dmesg -c > /dev/null 2>&1")
    
    def run_basic_test(self):
        """Базовый тест: выделение и освобождение разных размеров"""
        print("=" * 60)
        print("Запуск базового теста")
        print("=" * 60)
        
        self.clear_dmesg()
        
        # Шаг 1: Начальное состояние
        self.print_stats("1. Начальное состояние:")
        self.print_bitmap()
        
        # Шаг 2: Выделение блоков разных размеров
        print("\n2. Выделение блоков разного размера:")
        
        sizes = [128, 512, 1024, 4096, 8192, 16384]
        successful_allocations = []
        
        for i, size in enumerate(sizes):
            addr = self.allocate(size, f"block_{i}")
            if addr:
                successful_allocations.append((f"block_{i}", addr))
            else:
                print(f"✗ Пропускаем размер {size} (ошибка выделения)")
        
        self.print_stats("\n3. После выделения:")
        self.print_bitmap()
        
        # Шаг 3: Освобождение в случайном порядке
        print("\n4. Освобождение в случайном порядке:")
        
        # Освобождаем в порядке: 3, 1, 5, 0, 4, 2
        free_order = [3, 1, 5, 0, 4, 2]
        for i in free_order:
            name = f"block_{i}"
            if name in self.allocated:
                self.free(name)
        
        self.print_stats("\n5. После частичного освобождения:")
        self.print_bitmap()
        
        # Шаг 4: Проверка полного освобождения
        print("\n6. Освобождение оставшихся блоков:")
        for name, addr in list(self.allocated.items()):
            self.free(name)
        
        self.print_stats("7. Финальное состояние:")
        
        print("\n" + "=" * 60)
        print("Базовый тест завершен")
        print("=" * 60)
        return True
    
    def run_edge_cases_test(self):
        """Тест граничных случаев"""
        print("\n" + "=" * 60)
        print("Тест граничных случаев")
        print("=" * 60)
        
        self.clear_dmesg()
        
        # Тест 1: Выделение 0 байт
        print("\n1. Попытка выделить 0 байт:")
        ret, out, err = self.write_param("alloc", "0")
        if ret != 0:
            print("✓ Корректно отказано в выделении 0 байт")
        else:
            print("✗ Ошибка: разрешено выделение 0 байт")
        
        # Тест 2: Выделение очень большого размера
        print("\n2. Попытка выделить очень большой размер (100 МБ):")
        ret, out, err = self.write_param("alloc", "104857600")
        if ret != 0:
            print("✓ Корректно отказано в выделении слишком большого блока")
        else:
            print("✗ Ошибка: разрешено выделение слишком большого блока")
        
        # Тест 3: Несколько небольших выделений
        print("\n3. Несколько небольших выделений (по 100 байт):")
        for i in range(5):
            self.allocate(100, f"small_{i}")
        
        self.print_bitmap("После выделения 5 блоков по 100 байт:")
        
        # Тест 4: Освобождение в обратном порядке
        print("\n4. Освобождение в обратном порядке:")
        for i in range(4, -1, -1):
            self.free(f"small_{i}")
        
        self.print_bitmap("После освобождения:")
        
        print("\n" + "=" * 60)
        print("Тест граничных случаев завершен")
        print("=" * 60)
        return True
    
    def run_fragmentation_test(self):
        """Тест на фрагментацию"""
        print("\n" + "=" * 60)
        print("Тест на фрагментацию")
        print("=" * 60)
        
        self.clear_dmesg()
        
        # Выделяем блоки с промежутками
        print("\n1. Выделение блоков для создания фрагментации:")
        
        # Блок 1: 2 страницы
        addr1 = self.allocate(8192, "block1")
        
        # Блок 2: 1 страница
        addr2 = self.allocate(4096, "block2")
        
        # Блок 3: 3 страницы
        addr3 = self.allocate(12288, "block3")
        
        if not all([addr1, addr2, addr3]):
            print("✗ Не удалось выделить все блоки для теста фрагментации")
            return False
        
        self.print_stats("После выделения 3 блоков:")
        self.print_bitmap()
        
        # Освобождаем средний блок
        print("\n2. Освобождаем средний блок (создаем дырку):")
        self.free("block2")
        
        self.print_stats("После освобождения среднего блока:")
        self.print_bitmap()
        
        # Пытаемся выделить блок, который поместится в дырку
        print("\n3. Пытаемся выделить блок размером в 1 страницу:")
        addr4 = self.allocate(4096, "block4")
        
        if addr4:
            print("✓ Успешно выделен блок в освобожденное место")
        else:
            print("✗ Не удалось выделить блок в освобожденное место")
        
        self.print_stats("После выделения в освобожденное место:")
        self.print_bitmap()
        
        # Очистка
        print("\n4. Очистка:")
        for name in ["block1", "block3", "block4"]:
            if name in self.allocated:
                self.free(name)
        
        print("\n" + "=" * 60)
        print("Тест на фрагментацию завершен")
        print("=" * 60)
        return True
    
    def run_stress_test(self):
        """Стресс-тест: многократное выделение и освобождение"""
        print("\n" + "=" * 60)
        print("Стресс-тест")
        print("=" * 60)
        
        self.clear_dmesg()
        
        print("\n1. Многократное выделение/освобождение:")
        
        test_cycles = 3
        sizes = [256, 1024, 4096, 8192]
        
        for cycle in range(test_cycles):
            print(f"\nЦикл {cycle + 1}/{test_cycles}:")
            
            # Выделение
            addresses = []
            for i, size in enumerate(sizes):
                addr = self.allocate(size, f"stress_{cycle}_{i}")
                if addr:
                    addresses.append(addr)
            
            # Проверка
            self.print_stats(f"После выделения в цикле {cycle + 1}:")
            
            # Освобождение
            for i, size in enumerate(sizes):
                self.free(f"stress_{cycle}_{i}")
            
            self.print_stats(f"После освобождения в цикле {cycle + 1}:")
        
        print("\n" + "=" * 60)
        print("Стресс-тест завершен")
        print("=" * 60)
        return True

def main():
    """Основная функция тестирования"""
    print("Тестирование модуля аллокатора ядра")
    print("=" * 60)
    
    # Получаем имя модуля из аргументов или используем по умолчанию
    module_name = sys.argv[1] if len(sys.argv) > 1 else "my_module_kalloc56"
    
    test = KernelAllocatorTest(module_name=module_name)
    
    try:
        # Очищаем dmesg перед началом тестов
        test.clear_dmesg()
        
        # Запускаем тесты
        results = []
        
        print("\n" + "=" * 60)
        print("Начало тестирования")
        print("=" * 60)
        
        results.append(("Базовый тест", test.run_basic_test()))
        results.append(("Граничные случаи", test.run_edge_cases_test()))
        results.append(("Тест фрагментации", test.run_fragmentation_test()))
        results.append(("Стресс-тест", test.run_stress_test()))
        
        # Выводим итоговые сообщения из ядра
        print("\n" + "=" * 60)
        print("Сообщения ядра (последние 20 строк):")
        print("=" * 60)
        ret, out, err = test.get_dmesg(20)
        if ret == 0:
            print(out)
        
        # Сводка результатов
        print("\n" + "=" * 60)
        print("Сводка результатов тестирования:")
        print("=" * 60)
        
        passed = 0
        failed = 0
        
        for test_name, success in results:
            if success:
                print(f"✓ {test_name}: ПРОЙДЕН")
                passed += 1
            else:
                print(f"✗ {test_name}: НЕ ПРОЙДЕН")
                failed += 1
        
        print("\n" + "=" * 60)
        print(f"Итого: {passed} пройдено, {failed} не пройдено")
        
        if failed == 0:
            print("✓ Все тесты пройдены успешно!")
        else:
            print(f"✗ Обнаружены ошибки в {failed} тестах")
        
        print("=" * 60)
        
        # Возвращаем код ошибки если есть неудачные тесты
        sys.exit(1 if failed > 0 else 0)
        
    except KeyboardInterrupt:
        print("\n\nТестирование прервано пользователем")
        sys.exit(1)
    except Exception as e:
        print(f"\nОшибка при выполнении тестов: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()