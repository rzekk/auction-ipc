TEMPLATE = subdirs

# Перелік папок, які треба компілювати
SUBDIRS += \
    AuctionServer \
    AuctionClient

# (Опціонально) Задає порядок: спочатку сервер, потім клієнт (не обов'язково)
# AuctionClient.depends = AuctionServer