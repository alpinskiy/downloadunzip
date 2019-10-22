# DownloadUnzip

Скачивает и распаковывает ZIP файл в текущую директорию. В случае успеха - код возврата 0. В случае ошибки - код возврата 1 и сообщение в STDERR.

Поддерживает скачивание с веб-сайтов, требующих авторизации через ```application/x-www-form-urlencoded``` HTTP POST. Для этого нужно указать путь на сайте (относительный основному адресу сервера) и строку для отправки. Все полученые cookie будут переданы с GET запросом ZIP файла.

## Использование
```
  downloadunzip [Опции] URL
```

## Опции
```
  --login PATH
  POST request will be made here before downloading.
  All received cookies will be sent with the following GET request.
  Should be relative to URL host.

  --login-data DATA
  x-www-form-urlencoded data to be sent with login POST request.

  --overwrite
  Overwrite local files.

  --sha256 HEXSTRING
  SHA-256 digest to verify file integrity.

  --save
  Save ZIP file to disk.

  --dryrun
  Operate as usual but write nothing to disk.

  --verbose
  Set verbose mode on.
```
