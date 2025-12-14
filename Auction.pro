TEMPLATE = subdirs

# Перелік папок, які треба компілювати
SUBDIRS += \
    AuctionClient/AuctionClient \
    AuctionServer \
    AuctionClient \
    AuctionServer/AuctionServer

# (Опціонально) Задає порядок: спочатку сервер, потім клієнт (не обов'язково)
# AuctionClient.depends = AuctionServer