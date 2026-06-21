# HttpProxy (AsyncHttpProxy)

> Асинхронный HTTP-прокси-сервер на Boost.Asio и корутинах C++20.

Прокси принимает HTTP-запросы клиентов, извлекает целевой хост и порт из заголовков, устанавливает соединение с сервером, пересылает запрос и транслирует ответ обратно клиенту. Весь сетевой ввод-вывод неблокирующий и построен на **корутинах C++20** (`co_await`) поверх `boost::asio`.

## Возможности

- Приём подключений и обработка каждой сессии как отдельной корутины.
- Разбор HTTP-заголовков, извлечение `Host`/порта и `Content-Length`.
- Корректная пересылка тела ответа порциями (потоковая передача больших ответов с порогом буферизации).
- Асинхронное разрешение DNS, подключение, чтение и запись без блокировки потока.

## Технологии и концепции C++

- **Корутины C++20:** `boost::asio::awaitable<>`, `co_await`, `co_spawn`, `use_awaitable`.
- **Boost.Asio:** `io_context`, `tcp::socket`, `tcp::resolver`, `async_read_until`, `async_connect`, `async_read`, `async_write`, `dynamic_buffer`, `transfer_at_least`.
- Парсинг заголовков на `std::string_view` без лишних копий; `std::optional` для опциональных результатов.
- `std::function`-колбэк для итерации по заголовкам.

## Архитектура

```
main → io_context → co_spawn(session) на каждое подключение
session (awaitable<void>):
  read_until("\r\n\r\n")  → разбор заголовков (findHostPort)
  resolve + connect       → соединение с целевым сервером
  write(запрос)           → пересылка запроса
  read ответа             → findContentLength → потоковая пересылка тела клиенту
```

Вспомогательные функции парсинга (`iterHeaders`, `findHostPort`, `findContentLength`, `ParsePort`) вынесены в отдельный модуль и покрыты тестами.

## Стек

`C++20` · `Boost.Asio (coroutines)` · `CMake` · `GoogleTest`

## Сборка

```bash
mkdir build && cd build
cmake ..
make
```

## Запуск

Поднять прокси на порту 5555 и проверить через локальный сервер-заглушку:

```bash
cd build
./AsyncHttpProxy 5555 &

# тестовый сервер-заглушка
python3 -c 'print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 4096\r\n\r\n" + "A"*4096, end="")' | nc -l 127.0.0.1 -p 8000 &

# запрос через прокси
wget -e use_proxy=yes -e http_proxy=127.0.0.1:5555 127.0.0.1:8000
```

## Тесты

```bash
cd build
./AsyncHttpProxy_tests
```
