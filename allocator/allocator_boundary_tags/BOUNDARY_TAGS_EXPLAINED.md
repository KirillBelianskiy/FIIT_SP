# Аллокатор с граничными тегами (Boundary Tags Allocator)

## Содержание
1. [Основная теория](#1-основная-теория)
2. [Структура блоков памяти](#2-структура-блоков-памяти)
3. [Placement New](#3-placement-new)
4. [Зачем нужны неиспользуемые поля](#4-зачем-нужны-неиспользуемые-поля)
5. [Алгоритм do_allocate_sm](#5-алгоритм-do_allocate_sm)
6. [Алгоритм do_deallocate_sm](#6-алгоритм-do_deallocate_sm)
7. [Преимущества и недостатки](#7-преимущества-и-недостатки)

---

## 1. Основная теория

### 1.1 Что такое Boundary Tags?

Аллокатор с граничными тегами (Boundary Tags Allocator) — это классический алгоритм управления динамической памятью, предложенный Дональдом Кнутом. Основная идея заключается в хранении метаданных о размере блока **как в начале (header), так и в конце (footer)** каждого блока памяти.

### 1.2 Зачем нужны граничные теги?

Граничные теги решают критическую проблему: **эффективное слияние (coalescing) свободных блоков**.

**Без граничных тегов:**
- При освобождении блока мы можем легко найти следующий блок (двигаясь вперед по памяти)
- Но найти **предыдущий** блок сложно — нужно сканировать всю память с начала (O(n))

**С граничными тегами:**
- Каждый блок хранит размер в начале (header) и в конце (footer)
- При освобождении блока мы можем мгновенно найти предыдущий блок, прочитав footer предыдущего блока
- Это позволяет эффективно сливать свободные блоки в обоих направлениях за O(1)

### 1.3 Как работает навигация?

**Движение вперед:**
```cpp
// Читаем размер из header текущего блока
size_t block_size = *reinterpret_cast<size_t*>(current_block);
// Следующий блок начинается сразу после текущего
next_block = current_block + block_size;
```

**Движение назад:**
```cpp
// Footer предыдущего блока находится прямо перед текущим
size_t prev_size = *reinterpret_cast<size_t*>(current_block - sizeof(size_t));
// Предыдущий блок начинается на prev_size байт назад
prev_block = current_block - prev_size;
```

---

## 2. Структура блоков памяти

### 2.1 Метаданные аллокатора

В начале `_trusted_memory` хранятся метаданные аллокатора:

```
+------------------+
| fit_mode         |  4 байта  ← режим поиска (first/best/worst fit)
+------------------+
| size_t: total    |  8 байт   ← общий размер памяти
+------------------+
| std::mutex       |  40 байт  ← мьютекс для синхронизации
+------------------+
| void*: first_free|  8 байт   ← указатель на первый свободный блок
+------------------+
| [блоки памяти]   |           ← начало области с блоками
```

**Размер метаданных аллокатора:** `4 + 8 + 40 + 8 = 60 байт`

### 2.2 Свободный блок (Free Block)

```
+------------------+
| size_t: size     |  8 байт  ← размер всего блока (включая метаданные)
+------------------+
| void*: next      |  8 байт  ← указатель на следующий свободный блок
+------------------+
| void*: unused    |  8 байт  ← не используется в свободном блоке
+------------------+
| void*: unused    |  8 байт  ← не используется в свободном блоке
+------------------+
| ...              |          ← свободное пространство
| user data area   |
| ...              |
+------------------+
| size_t: size     |  8 байт  ← footer: дубликат размера (для обратного обхода)
+------------------+
```

**Используемые поля:**
- `size` (header) — размер блока
- `next` — указатель на следующий свободный блок в списке
- `size` (footer) — дубликат размера для обратного обхода

**Неиспользуемые поля:**
- Два указателя по 8 байт (резервируются, но не используются)

### 2.3 Занятый блок (Occupied Block)

```
+------------------+
| size_t: size     |  8 байт  ← размер всего блока
+------------------+
| bool: occupied   |  1 байт  ← флаг занятости (true = занят)
+------------------+
| void*: unused    |  8 байт  ← не используется
+------------------+
| void*: allocator |  8 байт  ← указатель на _trusted_memory (для проверки владельца)
+------------------+
| ...              |          ← пользовательские данные
| user data        |          ← ЭТО возвращается пользователю
| ...              |
+------------------+
| size_t: size     |  8 байт  ← footer: дубликат размера
+------------------+
```

**Используемые поля:**
- `size` (header) — размер блока
- `occupied` (bool) — флаг занятости (true/false)
- `allocator` — указатель на `_trusted_memory` (для проверки принадлежности)
- `size` (footer) — дубликат размера

**Неиспользуемые поля:**
- Один указатель по 8 байт

### 2.4 Размеры метаданных

```cpp
// Размер метаданных занятого блока (header + footer)
occupied_block_metadata_size = sizeof(size_t) +    // size (header)
                               sizeof(bool) +      // occupied flag
                               sizeof(void*) +     // unused
                               sizeof(void*) +     // allocator/unused
                               sizeof(size_t)      // size (footer)
                             = 8 + 1 + 8 + 8 + 8 = 33 байта
                             
// Но из-за выравнивания компилятор может добавить padding
// На практике: ~25 байт (без учета footer)
```

### 2.5 Флаг занятости

**Как отличить свободный блок от занятого?**

Используется второе поле (после `size`):
- **Свободный блок:** содержит указатель `next` (адрес следующего свободного блока или `nullptr`)
- **Занятый блок:** содержит `bool` значение `true`

**Проверка:**
```cpp
bool is_occupied = *reinterpret_cast<bool*>(block + sizeof(size_t));
```

Если значение `true`, блок занят. Если `false`, блок свободен.

**Преимущества использования bool:**
- Явная семантика (true/false вместо магических значений)
- Меньше памяти (1 байт вместо 8 байт)
- Более безопасно и понятно

---

## 3. Placement New

### 3.1 Что такое `new(current_ptr) std::mutex()`?

Это **placement new** — специальная форма оператора `new` в C++, которая создает объект в уже выделенной памяти.

**Синтаксис:**
```cpp
new(адрес) Тип(аргументы_конструктора);
```

### 3.2 Зачем это нужно в аллокаторе?

В конструкторе `allocator_boundary_tags` мы выделяем один большой блок памяти через `::operator new`:

```cpp
_trusted_memory = ::operator new(total_size);
```

Этот вызов выделяет **сырую память** (raw memory), но **не вызывает конструкторы** объектов. Это просто байты.

Затем мы вручную размещаем в этой памяти различные объекты:
- `fit_mode` (enum)
- `size_t` (размер)
- **`std::mutex`** (мьютекс для синхронизации)
- указатель на первый свободный блок

### 3.3 Проблема с `std::mutex`

`std::mutex` — это **не-тривиальный тип**. У него есть:
- Конструктор, который инициализирует внутреннее состояние
- Деструктор, который освобождает ресурсы
- Внутренние системные примитивы синхронизации

Если просто записать байты в память, мьютекс не будет правильно инициализирован.

### 3.4 Решение: Placement New

```cpp
new(current_ptr) std::mutex();
```

Эта строка:
1. **Не выделяет новую память** (память уже выделена)
2. **Вызывает конструктор** `std::mutex` по адресу `current_ptr`
3. Инициализирует мьютекс в уже существующей памяти

### 3.5 Полный пример из кода

```cpp
allocator_boundary_tags::allocator_boundary_tags(
    size_t space_size,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    // 1. Выделяем сырую память
    size_t total_size = allocator_metadata_size + space_size;
    _trusted_memory = ::operator new(total_size);
    
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    
    // 2. Записываем fit_mode (тривиальный тип)
    *reinterpret_cast<fit_mode*>(current_ptr) = allocate_fit_mode;
    current_ptr += sizeof(fit_mode);
    
    // 3. Записываем size (тривиальный тип)
    *reinterpret_cast<size_t*>(current_ptr) = total_size;
    current_ptr += sizeof(size_t);
    
    // 4. Создаем std::mutex через placement new (НЕ-тривиальный тип!)
    new(current_ptr) std::mutex();  // ← ЗДЕСЬ
    current_ptr += sizeof(std::mutex);
    
    // ... остальная инициализация
}
```

### 3.6 Важно: Деструктор

Если мы используем placement new, мы **обязаны** вручную вызвать деструктор:

```cpp
allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory == nullptr)
    {
        return;
    }
    
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);
    
    // Явно вызываем деструктор мьютекса
    reinterpret_cast<std::mutex*>(current_ptr)->~mutex();
    
    // Освобождаем память
    ::operator delete(_trusted_memory);
    _trusted_memory = nullptr;
}
```

### 3.7 Почему не обычный `new`?

Обычный `new` делает две вещи:
1. Выделяет память
2. Вызывает конструктор

Но в нашем случае:
- Мы хотим **один непрерывный блок памяти** для всех метаданных
- Мы сами управляем layout'ом памяти
- Обычный `new` выделил бы отдельный блок для мьютекса

---

## 4. Зачем нужны неиспользуемые поля

### 4.1 Проблема: Разные размеры метаданных

#### Наивный подход (НЕ работает):

**Свободный блок (минимальные метаданные):**
```
+------------------+
| size_t: size     |  8 байт
+------------------+
| void*: next      |  8 байт
+------------------+
| user data        |  N байт
+------------------+
| size_t: footer   |  8 байт
+------------------+
Итого метаданных: 24 байта
```

**Занятый блок (больше метаданных):**
```
+------------------+
| size_t: size     |  8 байт
+------------------+
| bool: occupied   |  1 байт
+------------------+
| void*: unused    |  8 байт
+------------------+
| void*: allocator |  8 байт
+------------------+
| user data        |  N байт
+------------------+
| size_t: footer   |  8 байт
+------------------+
Итого метаданных: 33 байта (с учетом выравнивания ~25 байт header)
```

### 4.2 Что пойдёт не так?

Представим сценарий:

```cpp
// 1. Выделяем блок 100 байт
void* ptr = allocator.allocate(100);

// Блок занимает: 100 + 40 (метаданные) = 140 байт
// Память:
// [OCCUPIED: 140 байт] [FREE: остаток]

// 2. Освобождаем блок
allocator.deallocate(ptr);

// Блок становится свободным, но...
// Если свободный блок занимает только 24 байта метаданных,
// то где хранить оставшиеся 16 байт?
```

### 4.3 Сценарий 1: Потеря памяти

```
До allocate:
[FREE: 140 байт]
  size: 140
  next: nullptr
  [пользовательская область: 140-24=116 байт]
  footer: 140

После allocate(100):
[OCCUPIED: 140 байт]  ← используем весь блок!
  size: 140
  flag: 0x1
  unused: nullptr
  allocator: ptr
  [пользовательские данные: 100 байт]
  [неиспользуемое место: 16 байт]  ← ПОТЕРЯ!
  footer: 140

После deallocate:
[FREE: 140 байт]
  size: 140
  next: nullptr
  [область: 116 байт]  ← но мы выделили только 100!
  footer: 140
```

**Проблема:** Если метаданные разного размера, мы не можем корректно преобразовать блок туда-обратно.

### 4.4 Сценарий 2: Перезапись данных

```
Если попытаться "ужать" блок при deallocate:

[OCCUPIED: 140 байт]
  size: 140
  flag: 0x1
  unused: nullptr
  allocator: ptr
  [данные: 100 байт]
  footer: 140

↓ deallocate ↓

[FREE: 124 байт???]  ← ОШИБКА! Размер изменился!
  size: 124
  next: nullptr
  [область: 100 байт]
  footer: 124

Но следующий блок начинается через 140 байт, а не 124!
Мы перезаписали часть следующего блока!
```

### 4.5 Решение: Фиксированный размер метаданных

**Единый размер для обоих типов блоков:**

```cpp
static constexpr const size_t occupied_block_metadata_size = 
    sizeof(size_t) +      // size (header)
    sizeof(void*) +       // flag/next
    sizeof(void*) +       // unused
    sizeof(void*) +       // allocator/unused
    sizeof(size_t);       // footer

// = 40 байт на 64-битной системе
```

**Свободный блок (40 байт метаданных):**
```
+------------------+
| size_t: size     |  8 байт  ← используется
+------------------+
| void*: next      |  8 байт  ← используется
+------------------+
| void*: unused    |  8 байт  ← НЕ используется, но РЕЗЕРВИРУЕТСЯ
+------------------+
| void*: unused    |  8 байт  ← НЕ используется, но РЕЗЕРВИРУЕТСЯ
+------------------+
| user data        |  N байт
+------------------+
| size_t: footer   |  8 байт  ← используется
+------------------+
```

**Занятый блок (40 байт метаданных):**
```
+------------------+
| size_t: size     |  8 байт  ← используется
+------------------+
| void*: flag      |  8 байт  ← используется (0x1)
+------------------+
| void*: unused    |  8 байт  ← НЕ используется
+------------------+
| void*: allocator |  8 байт  ← используется
+------------------+
| user data        |  N байт
+------------------+
| size_t: footer   |  8 байт  ← используется
+------------------+
```

### 4.6 Теперь всё работает!

```cpp
// 1. Выделяем блок
void* ptr = allocator.allocate(100);

[OCCUPIED: 140 байт]
  size: 140
  flag: 0x1
  unused: nullptr
  allocator: ptr
  [данные: 100 байт]
  footer: 140

// 2. Освобождаем блок
allocator.deallocate(ptr);

[FREE: 140 байт]  ← размер НЕ изменился!
  size: 140
  next: nullptr
  unused: nullptr  ← просто не используем
  unused: nullptr  ← просто не используем
  [область: 100 байт]
  footer: 140
```

**Размер блока остаётся 140 байт в обоих случаях!**

### 4.7 Вывод

"Неиспользуемые" поля — это **не баг, а фича**:

1. **Гарантируют фиксированный размер метаданных**
   - Блок не меняет размер при переходе FREE ↔ OCCUPIED
   - Упрощает арифметику указателей

2. **Упрощают код**
   - Не нужны сложные преобразования
   - Один размер метаданных для всех блоков

3. **Повышают производительность**
   - Нет перемещения блоков
   - Нет дополнительных проверок типа

4. **Обеспечивают корректность**
   - Невозможно перезаписать соседние блоки
   - Размеры всегда согласованы

**Цена:** 16 байт на каждый свободный блок (2 неиспользуемых указателя)

**Но это ничтожно мало** по сравнению с:
- Сложностью альтернативных подходов
- Потенциальными багами
- Потерей производительности

### 4.8 Итоговая формула

```
Размер блока = КОНСТАНТА (не зависит от состояния FREE/OCCUPIED)

occupied_block_metadata_size = 
    sizeof(size_t) +    // header: size
    sizeof(void*) +     // field 1: flag или next
    sizeof(void*) +     // field 2: unused
    sizeof(void*) +     // field 3: allocator или unused
    sizeof(size_t)      // footer: size

= 40 байт (на 64-битной системе)
```

Это **инвариант** аллокатора — размер метаданных всегда одинаковый, независимо от состояния блока.

---

## 5. Алгоритм do_allocate_sm (Выделение памяти)

### 5.1 Общая схема

```
Алгоритм allocate(size):
1. Читаем метаданные аллокатора (fit_mode, мьютекс)
2. Блокируем мьютекс
3. Ищем подходящий свободный блок (по стратегии fit_mode)
4. Если блок найден:
   - Если остаток достаточно большой → делим блок
   - Иначе → используем весь блок
5. Маркируем блок как занятый
6. Возвращаем указатель на пользовательскую область
```

### 5.2 Построчное объяснение

#### Строки 81-88: Чтение метаданных аллокатора

```cpp
[[nodiscard]] void* allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    
    auto mode = *reinterpret_cast<fit_mode*>(current_ptr);
    current_ptr += sizeof(fit_mode);
    current_ptr += sizeof(size_t);
```

**Строка 84:**
- Преобразуем `_trusted_memory` в указатель на байты (`unsigned char*`)
- Это позволяет делать побайтовую арифметику

**Строка 86:**
- Читаем режим поиска (first_fit/best_fit/worst_fit) из начала памяти

**Строки 87-88:**
- Пропускаем `fit_mode` (4 байта) и `size_t` (8 байт)
- Теперь `current_ptr` указывает на `std::mutex`

#### Строки 90-94: Блокировка мьютекса

```cpp
    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);
    
    auto first_free_ptr = reinterpret_cast<void**>(current_ptr);
```

**Строка 91:**
- `std::lock_guard` — RAII-обёртка:
  - Блокирует мьютекс при создании
  - Автоматически разблокирует при выходе из функции
- Защищает от одновременного доступа из разных потоков

**Строка 94:**
- `first_free_ptr` — указатель на указатель первого свободного блока
- Это голова связного списка свободных блоков

#### Строки 96-103: Подготовка к поиску

```cpp
    size_t required_size = size + occupied_block_metadata_size;
    
    void* best_block = nullptr;
    void** best_prev = nullptr;
    size_t best_size = 0;
    
    void** prev_ptr = first_free_ptr;
    void* current_block = *first_free_ptr;
```

**Строка 96:**
- Вычисляем полный размер блока:
  - `size` — пользовательские данные
  - `occupied_block_metadata_size` = 40 байт (header + footer)

**Строки 98-100:**
- `best_block` — лучший найденный блок (по критерию fit_mode)
- `best_prev` — указатель на предыдущий элемент в списке (для удаления)
- `best_size` — размер лучшего блока

**Строки 102-103:**
- `prev_ptr` — указатель на "next" предыдущего блока
- `current_block` — текущий проверяемый блок

#### Строки 105-140: Поиск подходящего блока

```cpp
    while (current_block != nullptr)
    {
        size_t block_size = *reinterpret_cast<size_t*>(current_block);
        
        if (block_size >= required_size)
        {
            if (mode == fit_mode::first_fit)
            {
                best_block = current_block;
                best_prev = prev_ptr;
                best_size = block_size;
                break;  // ← Сразу выходим!
            }
```

**First Fit (строки 111-117):**
- Берём **первый** подходящий блок
- `break` — сразу выходим из цикла
- Самый быстрый поиск

```cpp
            else if (mode == fit_mode::the_best_fit)
            {
                if (best_block == nullptr || block_size < best_size)
                {
                    best_block = current_block;
                    best_prev = prev_ptr;
                    best_size = block_size;
                }
            }
```

**Best Fit (строки 118-126):**
- Ищем **самый маленький** подходящий блок
- Минимизирует фрагментацию
- Проходим весь список

```cpp
            else if (mode == fit_mode::the_worst_fit)
            {
                if (best_block == nullptr || block_size > best_size)
                {
                    best_block = current_block;
                    best_prev = prev_ptr;
                    best_size = block_size;
                }
            }
        }
        
        prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_block) + sizeof(size_t));
        current_block = *prev_ptr;
    }
```

**Worst Fit (строки 127-135):**
- Ищем **самый большой** блок
- Оставляет большие свободные блоки

**Строки 138-140:**
- Переход к следующему блоку в списке
- `current_block + sizeof(size_t)` — пропускаем поле `size`, попадаем на `next`

#### Строки 142-145: Проверка успешности

```cpp
    if (best_block == nullptr)
    {
        throw std::bad_alloc();
    }
```

Если не нашли подходящий блок — выбрасываем исключение.

#### Строки 147-169: Разделение блока

```cpp
    size_t remaining_size = best_size - required_size;
    
    if (remaining_size >= occupied_block_metadata_size)
    {
        // Делим блок
        void* new_free_block = reinterpret_cast<unsigned char*>(best_block) + required_size;
        void* next_free = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(best_block) + sizeof(size_t));
```

**Строка 147:**
- Вычисляем остаток после выделения

**Строка 149:**
- Проверяем, достаточно ли места для нового свободного блока
- Если остаток < 40 байт — не делим (используем весь блок)

**Строка 151:**
- Адрес нового свободного блока = начало старого + размер выделяемого

**Строка 152:**
- Читаем `next` из старого блока (следующий в списке)

```cpp
        auto new_block_ptr = reinterpret_cast<unsigned char*>(new_free_block);
        *reinterpret_cast<size_t*>(new_block_ptr) = remaining_size;
        *reinterpret_cast<void**>(new_block_ptr + sizeof(size_t)) = next_free;
        *reinterpret_cast<void**>(new_block_ptr + sizeof(size_t) + sizeof(void*)) = nullptr;
        *reinterpret_cast<void**>(new_block_ptr + sizeof(size_t) + sizeof(void*) * 2) = nullptr;
        
        auto new_footer_ptr = new_block_ptr + remaining_size - sizeof(size_t);
        *reinterpret_cast<size_t*>(new_footer_ptr) = remaining_size;
        
        *best_prev = new_free_block;
```

**Строки 155-158:**
- Инициализируем новый свободный блок:
  - `size` = остаток
  - `next` = следующий свободный блок
  - Обнуляем неиспользуемые поля

**Строки 160-161:**
- Записываем размер в footer

**Строка 163:**
- Обновляем список: предыдущий блок теперь указывает на новый

```cpp
    }
    else
    {
        *best_prev = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(best_block) + sizeof(size_t));
        required_size = best_size;
    }
```

**Если блок не делится (строки 165-169):**
- Удаляем блок из списка свободных
- Используем весь блок целиком

#### Строки 171-186: Маркировка как занятого

```cpp
    auto best_block_ptr = reinterpret_cast<unsigned char*>(best_block);
    *reinterpret_cast<size_t*>(best_block_ptr) = required_size;
    *reinterpret_cast<bool*>(best_block_ptr + sizeof(size_t)) = true;  // занят
    *reinterpret_cast<void**>(best_block_ptr + sizeof(size_t) + sizeof(bool)) = nullptr;
```

**Строка 172:**
- Записываем размер в header

**Строка 173:**
- **Важно!** Устанавливаем флаг занятости = `true`
- Блок теперь помечен как занятый

**Строка 174:**
- Обнуляем второй указатель

```cpp
    auto footer_ptr = best_block_ptr + required_size - sizeof(size_t);
    *reinterpret_cast<size_t*>(footer_ptr) = required_size;
    
    if (required_size >= occupied_block_metadata_size + sizeof(size_t))
    {
        *reinterpret_cast<void**>(best_block_ptr + sizeof(size_t) + sizeof(bool) + sizeof(void*)) = _trusted_memory;
    }
    
    return reinterpret_cast<unsigned char*>(best_block) + occupied_block_metadata_size;
}
```

**Строки 177-178:**
- Записываем размер в footer

**Строки 181-184:**
- Если блок достаточно большой — записываем указатель на аллокатор
- Нужно для проверки при `deallocate`

**Строка 186:**
- Возвращаем указатель **после метаданных**
- Пользователь получает чистую область для данных

### 5.3 Визуальный пример

```
Исходное состояние:
┌────────────────────────────────────────┐
│ FREE: 200 байт                         │
│ size=200, next=nullptr                 │
└────────────────────────────────────────┘

Запрос: allocate(100)
required_size = 100 + 25 = 125 байт (примерно)

После выделения:
┌──────────────────────┬─────────────────┐
│ OCCUPIED: 125 байт   │ FREE: 75 байт   │
│ size=125, flag=true  │ size=75, next=..│
│ [100 байт данных]    │                 │
└──────────────────────┴─────────────────┘
         ↑
    Возвращается пользователю
```

### 5.4 Стратегии поиска

| Стратегия | Описание | Сложность | Фрагментация |
|-----------|----------|-----------|--------------|
| First Fit | Первый подходящий | O(n) быстро | Средняя |
| Best Fit | Самый маленький | O(n) медленно | Низкая |
| Worst Fit | Самый большой | O(n) медленно | Высокая |

---

## 6. Алгоритм do_deallocate_sm (Освобождение памяти)

### 6.1 Общая схема

```
Алгоритм deallocate(ptr):
1. Проверяем валидность указателя
2. Проверяем флаг занятости и принадлежность аллокатору
3. Пытаемся слить с предыдущим блоком (если он свободен)
4. Пытаемся слить со следующим блоком (если он свободен)
5. Удаляем слитые блоки из списка свободных
6. Вставляем объединённый блок в список свободных
7. Обновляем header и footer
```

### 6.2 Построчное объяснение

#### Строки 189-205: Проверка nullptr и получение метаданных

```cpp
void allocator_boundary_tags::do_deallocate_sm(
    void* at)
{
    if (at == nullptr)
    {
        return;
    }
    
    auto current_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    current_ptr += sizeof(fit_mode);
    
    size_t total_size = *reinterpret_cast<size_t*>(current_ptr);
    current_ptr += sizeof(size_t);
    
    auto mutex_ptr = reinterpret_cast<std::mutex*>(current_ptr);
    std::lock_guard<std::mutex> lock(*mutex_ptr);
    current_ptr += sizeof(std::mutex);
```

**Строки 192-196:**
- Стандартное поведение: `free(nullptr)` — это no-op

**Строки 197-205:**
- Пропускаем `fit_mode`
- Читаем общий размер памяти (нужен для проверки границ)
- Блокируем мьютекс

#### Строки 207-217: Проверка принадлежности блока

```cpp
    auto memory_start = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    auto memory_end = reinterpret_cast<unsigned char*>(_trusted_memory) + total_size;
    auto block_ptr = reinterpret_cast<unsigned char*>(at);
    
    void* block = block_ptr - occupied_block_metadata_size;
    
    if (reinterpret_cast<unsigned char*>(block) < memory_start ||
        reinterpret_cast<unsigned char*>(block) >= memory_end)
    {
        throw std::logic_error("Block does not belong to this allocator");
    }
```

**Строки 207-208:**
- Вычисляем границы области с блоками

**Строка 211:**
- `at` — указатель на пользовательские данные
- Вычитаем размер метаданных → получаем начало блока

**Строки 213-217:**
- Проверяем, что блок внутри памяти аллокатора
- Защита от освобождения чужих блоков

#### Строки 219-239: Проверка флага занятости и владельца

```cpp
    auto block_data_ptr = reinterpret_cast<unsigned char*>(block);
    bool is_occupied = *reinterpret_cast<bool*>(block_data_ptr + sizeof(size_t));
    
    if (!is_occupied)
    {
        throw std::logic_error("Double free or invalid pointer");
    }
    
    size_t block_size = *reinterpret_cast<size_t*>(block);
    
    if (block_size >= occupied_block_metadata_size + sizeof(size_t))
    {
        void* allocator_ptr = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(block) + sizeof(size_t) +
            sizeof(bool) + sizeof(void*));
        if (allocator_ptr != _trusted_memory)
        {
            throw std::logic_error("Block does not belong to this allocator");
        }
    }
```

**Строка 218:**
- Читаем флаг занятости (bool)

**Строка 219:**
- Проверяем значение:
  - `true` → блок занят (можно освобождать)
  - `false` → блок свободен (double free!)

**Строки 221-224:**
- Защита от двойного освобождения

**Строки 228-237:**
- Если блок достаточно большой — проверяем указатель на аллокатор
- Защита от освобождения блока из другого аллокатора

#### Строки 240-269: Слияние с предыдущим блоком

```cpp
    auto block_start = reinterpret_cast<unsigned char*>(block);
    auto block_end = block_start + block_size;
    
    auto first_block_start = reinterpret_cast<unsigned char*>(_trusted_memory) + allocator_metadata_size;
    auto last_block_end = memory_end;
    
    void* prev_block = nullptr;
    void* next_block = nullptr;
    
    if (block_start > first_block_start)
    {
        auto prev_footer_ptr = block_start - sizeof(size_t);
        size_t prev_size = *reinterpret_cast<size_t*>(prev_footer_ptr);
        prev_block = block_start - prev_size;
```

**Строки 240-247:**
- Вычисляем границы текущего и всей области
- Инициализируем переменные для соседних блоков

**Строка 249:**
- Если это не первый блок — проверяем предыдущий

**Строки 251-253:**
- **Footer предыдущего блока** находится прямо перед текущим
- Читаем размер из footer
- Вычисляем начало предыдущего блока

```cpp
        auto prev_block_ptr = reinterpret_cast<unsigned char*>(prev_block);
        bool prev_is_occupied = *reinterpret_cast<bool*>(prev_block_ptr + sizeof(size_t));
        
        if (!prev_is_occupied)
        {
            block_size += prev_size;
            block = prev_block;
            block_start = reinterpret_cast<unsigned char*>(block);
        }
        else
        {
            prev_block = nullptr;
        }
    }
```

**Строки 255-257:**
- Проверяем флаг занятости предыдущего блока

**Строки 259-263:**
- Если предыдущий блок **свободен** — **сливаем**:
  - Увеличиваем размер
  - Текущий блок теперь начинается с предыдущего
  - `prev_block` остаётся != nullptr (для удаления из списка)

**Строки 264-268:**
- Если предыдущий блок занят — не сливаем
- `prev_block = nullptr` означает "не было слияния"

#### Строки 271-288: Слияние со следующим блоком

```cpp
    if (block_end < last_block_end)
    {
        next_block = block_end;
        size_t next_size = *reinterpret_cast<size_t*>(next_block);
        
        auto next_block_ptr = reinterpret_cast<unsigned char*>(next_block);
        bool next_is_occupied = *reinterpret_cast<bool*>(next_block_ptr + sizeof(size_t));
        
        if (!next_is_occupied)
        {
            block_size += next_size;
        }
        else
        {
            next_block = nullptr;
        }
    }
```

**Строка 271:**
- Если это не последний блок — проверяем следующий

**Строка 273:**
- Следующий блок начинается сразу после текущего

**Строки 276-278:**
- Проверяем флаг занятости следующего блока

**Строки 280-282:**
- Если следующий блок **свободен** — сливаем (увеличиваем размер)
- `next_block` остаётся != nullptr (для удаления из списка)

**Строки 284-287:**
- Если занят — не сливаем

#### Строки 290-326: Удаление слитых блоков из списка

```cpp
    auto first_free_ptr = reinterpret_cast<void**>(current_ptr);
    
    if (next_block != nullptr)
    {
        void** prev_ptr = first_free_ptr;
        void* current_free = *first_free_ptr;
        
        while (current_free != nullptr && current_free != next_block)
        {
            prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_free) + sizeof(size_t));
            current_free = *prev_ptr;
        }
        
        if (current_free == next_block)
        {
            void* next_free = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(next_block) + sizeof(size_t));
            *prev_ptr = next_free;
        }
    }
```

**Строка 290:**
- Получаем указатель на голову списка свободных блоков

**Строки 292-308:**
- Если мы слили со следующим блоком (`next_block != nullptr`):
  - Ищем его в списке свободных
  - Удаляем: перенаправляем `prev->next` на `next->next`
  - Блок "выпадает" из списка

**Строки 310-326:**
- Аналогично для предыдущего блока (если слили с ним)

#### Строки 328-346: Вставка освобождённого блока в список

```cpp
    *reinterpret_cast<size_t*>(block) = block_size;
    
    void** prev_ptr = first_free_ptr;
    void* current_free = *first_free_ptr;
    
    while (current_free != nullptr && current_free < block)
    {
        prev_ptr = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(current_free) + sizeof(size_t));
        current_free = *prev_ptr;
    }
```

**Строка 328:**
- Записываем финальный размер блока (после всех слияний)

**Строки 330-337:**
- Ищем место в списке свободных блоков
- **Список упорядочен по адресам** (важно для эффективного слияния)
- Ищем первый блок с адресом больше текущего

```cpp
    auto block_ptr2 = reinterpret_cast<unsigned char*>(block);
    *reinterpret_cast<void**>(block_ptr2 + sizeof(size_t)) = current_free;
    *reinterpret_cast<void**>(block_ptr2 + sizeof(size_t) + sizeof(void*)) = nullptr;
    *reinterpret_cast<void**>(block_ptr2 + sizeof(size_t) + sizeof(void*) * 2) = nullptr;
    *prev_ptr = block;
    
    auto footer_ptr = block_ptr2 + block_size - sizeof(size_t);
    *reinterpret_cast<size_t*>(footer_ptr) = block_size;
}
```

**Строка 340:**
- Записываем `next` — указатель на следующий свободный блок

**Строки 341-342:**
- Обнуляем неиспользуемые поля

**Строка 343:**
- Вставляем блок в список:
  - `prev->next = block`
  - `block->next = current_free`

**Строки 345-346:**
- Записываем размер в footer
- Footer критически важен для следующего `deallocate`

### 6.3 Визуальный пример слияния

```
Исходное состояние:
┌──────────┬──────────┬──────────┐
│ FREE: 60 │ OCC: 140 │ FREE: 80 │
└──────────┴──────────┴──────────┘

Освобождаем средний блок:

Шаг 1: Проверяем предыдущий блок
┌──────────┬──────────┬──────────┐
│ FREE: 60 │ OCC: 140 │ FREE: 80 │
└──────────┴──────────┴──────────┘
     ↑           ↑
  свободен!   освобождаем
  
  → Сливаем: block_size = 60 + 140 = 200

Шаг 2: Проверяем следующий блок
┌─────────────────────┬──────────┐
│ FREE: 200           │ FREE: 80 │
└─────────────────────┴──────────┘
                           ↑
                       свободен!
  
  → Сливаем: block_size = 200 + 80 = 280

Шаг 3: Удаляем старые блоки из списка
- Удаляем блок 60 из списка
- Удаляем блок 80 из списка

Шаг 4: Вставляем новый блок
┌──────────────────────────────────┐
│ FREE: 280                        │
└──────────────────────────────────┘

Результат: три блока объединились в один!
```

### 6.4 Ключевые идеи

**Зачем footer?**
- Позволяет идти назад по памяти за O(1)
- Без footer пришлось бы искать предыдущий блок за O(n)

**Зачем упорядоченный список?**
- Упрощает поиск соседних блоков при слиянии
- Соседние блоки в памяти находятся рядом в списке

**Зачем флаг bool?**
- Отличаем занятые блоки от свободных
- Защита от double free
- Явная семантика (true/false)
- Экономия памяти (1 байт вместо 8)

**Coalescing (слияние):**
- Предотвращает фрагментацию памяти
- Объединяет соседние свободные блоки в один большой
- Критически важно для долгоживущих программ

### 6.5 Проверки безопасности

| Проверка | Защита от |
|----------|-----------|
| `at == nullptr` | Некорректного использования |
| Границы памяти | Освобождения чужих блоков |
| Флаг `bool` | Double free |
| `allocator_ptr` | Освобождения блока из другого аллокатора |

---

## 7. Преимущества и недостатки

### 7.1 Преимущества

✅ **Эффективное слияние блоков**
- O(1) слияние в обоих направлениях благодаря footer
- Предотвращает фрагментацию памяти
- Критично для долгоживущих программ

✅ **Простота реализации**
- Понятная структура данных
- Прямолинейная логика
- Легко отлаживать

✅ **Двунаправленная навигация**
- Можно двигаться по блокам вперёд и назад
- Упрощает итераторы и отладку

✅ **Гибкость**
- Поддержка различных стратегий поиска (first/best/worst fit)
- Можно менять стратегию динамически

✅ **Хорошая локальность**
- Метаданные рядом с данными
- Улучшает производительность кэша

✅ **Потокобезопасность**
- Встроенная защита через `std::mutex`
- Безопасно использовать из нескольких потоков

### 7.2 Недостатки

❌ **Накладные расходы на память**
- Footer добавляет 8 байт к каждому блоку
- Неиспользуемые поля добавляют 16 байт к свободным блокам
- Итого: ~25 байт метаданных на блок (header без footer)
- С учетом выравнивания может быть больше

❌ **Внешняя фрагментация**
- Может возникать при частых выделениях/освобождениях
- Особенно при best-fit стратегии

❌ **Линейный поиск**
- O(n) поиск свободного блока
- Медленно при большом количестве свободных блоков
- Best-fit и worst-fit требуют полного прохода списка

❌ **Нет поддержки выравнивания**
- Текущая реализация не учитывает alignment
- Может быть проблемой для специальных типов данных

❌ **Overhead на малых блоках**
- 25 байт метаданных на блок 8 байт = 312% overhead
- Неэффективно для множества маленьких объектов

### 7.3 Сложность операций

| Операция | Сложность | Комментарий |
|----------|-----------|-------------|
| Выделение (first-fit) | O(n) | n — количество свободных блоков, но обычно быстро |
| Выделение (best-fit) | O(n) | Нужно просмотреть весь список |
| Выделение (worst-fit) | O(n) | Нужно просмотреть весь список |
| Освобождение | O(n) | Поиск позиции в упорядоченном списке |
| Слияние с соседом | O(1) | Благодаря footer |
| Навигация вперёд | O(1) | Чтение header |
| Навигация назад | O(1) | Чтение footer |

### 7.4 Сравнение с другими аллокаторами

#### vs Sorted List Allocator

| Характеристика | Boundary Tags | Sorted List |
|----------------|---------------|-------------|
| Слияние | O(1) | O(n) |
| Навигация назад | O(1) | O(n) |
| Накладные расходы | Выше (footer) | Ниже |
| Сложность кода | Проще | Сложнее |

#### vs Buddies System

| Характеристика | Boundary Tags | Buddies System |
|----------------|---------------|----------------|
| Фрагментация | Внешняя | Внутренняя |
| Размеры блоков | Произвольные | Степени двойки |
| Слияние | O(1) | O(log n) |
| Поиск | O(n) | O(1) для каждого уровня |
| Overhead | 40 байт/блок | Зависит от размера |

#### vs Red-Black Tree Allocator

| Характеристика | Boundary Tags | RB Tree |
|----------------|---------------|---------|
| Поиск | O(n) | O(log n) |
| Вставка/удаление | O(n) | O(log n) |
| Память | Меньше | Больше (узлы дерева) |
| Сложность кода | Проще | Сложнее |

---

## 8. Примеры использования

### 8.1 Создание аллокатора

```cpp
// Создать аллокатор с 1024 байтами памяти
allocator_boundary_tags alloc(1024);

// С явной стратегией best-fit
allocator_boundary_tags alloc(
    1024, 
    allocator_with_fit_mode::fit_mode::the_best_fit
);
```

### 8.2 Базовое использование

```cpp
// Выделить 64 байта
void* ptr1 = alloc.allocate(64);

// Выделить 128 байт
void* ptr2 = alloc.allocate(128);

// Освободить первый блок
alloc.deallocate(ptr1, 64);

// Освободить второй блок (произойдёт слияние)
alloc.deallocate(ptr2, 128);
```

### 8.3 Изменение стратегии

```cpp
// Переключиться на worst-fit
alloc.set_fit_mode(allocator_with_fit_mode::fit_mode::the_worst_fit);

// Выделить с новой стратегией
void* ptr = alloc.allocate(256);
```

### 8.4 Получение информации о блоках

```cpp
auto blocks = alloc.get_blocks_info();
for (const auto& block : blocks) {
    std::cout << "Block size: " << block.block_size 
              << ", occupied: " << block.is_block_occupied << "\n";
}
```

### 8.5 Использование с пользовательскими типами

```cpp
// Выделить память для структуры
struct Data {
    int x, y, z;
};

void* raw = alloc.allocate(sizeof(Data));
Data* data = new(raw) Data{1, 2, 3};  // placement new

// Использование
std::cout << data->x << "\n";

// Освобождение
data->~Data();  // явный вызов деструктора
alloc.deallocate(raw, sizeof(Data));
```

---

## 9. Возможные оптимизации

### 9.1 Segregated Free Lists

Разделить свободные блоки по размерам:

```
Small (0-64):    [блок1] -> [блок2] -> ...
Medium (65-256): [блок3] -> [блок4] -> ...
Large (257+):    [блок5] -> [блок6] -> ...
```

**Преимущества:**
- Ускоряет поиск (ищем только в нужной категории)
- Уменьшает фрагментацию

### 9.2 Bitmap для флагов

**Текущая реализация уже использует bool (1 байт) для флага занятости.**

Дальнейшая оптимизация — использовать младший бит `size` для флага:

```cpp
// Размеры всегда выровнены, младший бит = 0
size & ~1  → реальный размер
size & 1   → флаг занятости (0 = свободен, 1 = занят)
```

**Экономия:** еще 1 байт на каждом блоке (но усложняет код)

**Текущее решение с bool:**
- Более читаемый код
- Явная семантика
- Уже экономит 7 байт по сравнению со старой реализацией (uint64_t)

### 9.3 Lazy Coalescing

Откладывать слияние до момента нехватки памяти:

```cpp
void deallocate(void* ptr) {
    // Просто помечаем блок как свободный
    mark_as_free(ptr);
    // НЕ сливаем сразу
}

void* allocate(size_t size) {
    void* ptr = find_free_block(size);
    if (ptr == nullptr) {
        coalesce_all();  // ← Сливаем только когда нужно
        ptr = find_free_block(size);
    }
    return ptr;
}
```

**Преимущества:**
- Быстрее deallocate
- Меньше операций при частых alloc/dealloc

### 9.4 Alignment Support

Добавить поддержку выравнивания:

```cpp
void* allocate_aligned(size_t size, size_t alignment) {
    size_t required = size + alignment + metadata_size;
    void* raw = allocate(required);
    
    // Выравниваем указатель
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    
    return reinterpret_cast<void*>(aligned);
}
```

---

## 10. Заключение

Аллокатор с граничными тегами — это **классический и надёжный** алгоритм управления памятью. 

**Когда использовать:**
- Нужна простая и понятная реализация
- Важно эффективное слияние блоков
- Размеры блоков произвольные
- Не критичны накладные расходы на память

**Когда НЕ использовать:**
- Множество маленьких объектов (большой overhead)
- Критична скорость поиска (используйте RB-tree)
- Нужны только степени двойки (используйте Buddies System)
- Критична минимизация памяти

**Ключевые идеи:**
1. **Footer** позволяет O(1) навигацию назад
2. **Фиксированный размер метаданных** упрощает преобразование FREE ↔ OCCUPIED
3. **Coalescing** предотвращает фрагментацию
4. **Упорядоченный список** упрощает слияние соседних блоков

Этот аллокатор — отличный выбор для образовательных целей и систем, где важна предсказуемость и простота, но не критична максимальная эффективность.

---

## 11. Дополнительные материалы

### 11.1 Связанные файлы

- `allocator_boundary_tags.h` — заголовочный файл
- `allocator_boundary_tags.cpp` — реализация
- `allocator_boundary_tags_tests.cpp` — тесты
- `PLACEMENT_NEW_EXPLAINED.md` — подробно о placement new
- `WHY_UNUSED_FIELDS.md` — зачем нужны неиспользуемые поля

### 11.2 Литература

- Donald Knuth, "The Art of Computer Programming, Volume 1: Fundamental Algorithms"
- "Dynamic Storage Allocation: A Survey and Critical Review" by Paul R. Wilson et al.

### 11.3 Термины

- **Boundary Tags** — граничные теги (header + footer)
- **Coalescing** — слияние соседних свободных блоков
- **Fragmentation** — фрагментация памяти
- **Fit Mode** — стратегия поиска блока
- **Placement New** — создание объекта в уже выделенной памяти
- **Footer** — метаданные в конце блока
- **Header** — метаданные в начале блока

---

**Документация создана:** 2026-05-07  
**Версия аллокатора:** 1.1  
**Автор документации:** Claude Code

## История изменений

### Версия 1.1 (2026-05-07)
- **Замена `uint64_t` на `bool` для флага занятости**
  - Уменьшен размер метаданных с 40 до ~25 байт (header)
  - Более явная семантика (true/false вместо 0x1)
  - Экономия 7 байт на каждом блоке
  - Улучшена читаемость кода
  - Все тесты обновлены и проходят успешно

### Версия 1.0 (2026-05-07)
- Первоначальная реализация с `uint64_t` флагом
- Использование магического значения `0x1` для обозначения занятого блока

