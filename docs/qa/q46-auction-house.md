# Q46: 如何设计拍卖行？

## 问题分析

本题考察对拍卖行系统的理解：
- 拍卖行数据模型
- 竞价机制设计
- 拍卖行搜索与排序
- 大量数据的性能优化

---

## 一、拍卖行系统架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    拍卖行系统架构                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  卖家流程:                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家上架物品                                   │       │
│  │     ↓                                            │       │
│  │  2. 设置起始价 / 一口价                            │       │
│  │     ↓                                            │       │
│  │  3. 设置拍卖时长 (8/24/48小时)                     │       │
│  │     ↓                                            │       │
│  │  4. 支付上架费                                     │       │
│  │     ↓                                            │       │
│  │  5. 物品进入拍卖行                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  买家流程:                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 浏览拍卖行 (搜索、筛选、排序)                    │       │
│  │     ↓                                            │       │
│  │  2. 竞价出价 / 一口价购买                           │       │
│  │     ↓                                            │       │
│  │  3. 扣除金币/钻石                                   │       │
│  │     ↓                                            │       │
│  │  4. 竞价成功或被超价则通知                          │       │
│  │     ↓                                            │       │
│  │  5. 拍卖结束获得物品                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  系统功能:                                                   │
│  ├── 物品上架/下架                                          │
│  ├── 竞价系统                                              │
│  ├── 一口价购买                                            │
│  ├── 搜索与筛选                                            │
│  ├── 价格排序                                              │
│  ├── 到期处理                                              │
│  ├── 邮件发送                                              │
│  └── 手续费收取                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 拍卖类型

```
┌─────────────────────────────────────────────────────────────┐
│                    拍卖类型                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 竞价拍卖 (Auction)                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  设定起始价，玩家竞价，价高者得                     │       │
│  │                                                   │       │
│  │  示例:                                            │       │
│  │  - 起始价: 100金币                                 │       │
│  │  - 玩家A出价: 100金币                              │       │
│  │  - 玩家B出价: 150金币 (当前最高)                    │       │
│  │  - 玩家C出价: 200金币 (当前最高)                    │       │
│  │  - 拍卖结束: 玩家C获得物品，玩家B收到退款           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 一口价 (Buyout)                                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │  设定固定价格，先到先得                             │       │
│  │                                                   │       │
│  │  示例:                                            │       │
│  │  - 一口价: 500金币                                 │       │
│  │  - 玩家A点击购买 → 立即获得物品                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 竞价+一口价 (Auction + Buyout)                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  可以竞价，也可以直接一口价购买                     │       │
│  │                                                   │       │
│  │  示例:                                            │       │
│  │  - 起始价: 100金币                                 │       │
│  │  - 一口价: 500金币                                 │       │
│  │  - 玩家可以选择竞价或直接购买                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据模型设计

### 2.1 拍卖行表结构

```sql
-- 拍卖行主表
CREATE TABLE `auction_house` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `seller_id` BIGINT UNSIGNED NOT NULL COMMENT '卖家ID',
    `seller_name` VARCHAR(64) NOT NULL COMMENT '卖家名称',
    `item_id` INT UNSIGNED NOT NULL COMMENT '物品模板ID',
    `item_unique_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '唯一物品ID',
    `item_count` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '物品数量',
    `item_quality` TINYINT UNSIGNED DEFAULT 0 COMMENT '物品品质',
    `item_enchant` TINYINT UNSIGNED DEFAULT 0 COMMENT '强化等级',
    `start_price` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '起始价格',
    `buyout_price` INT UNSIGNED DEFAULT NULL COMMENT '一口价格',
    `current_bid` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前最高出价',
    `current_bidder_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '当前出价者ID',
    `current_bidder_name` VARCHAR(64) DEFAULT NULL COMMENT '当前出价者名称',
    `min_bid_increment` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '最小加价幅度',
    `auction_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0:竞价,1:一口价,2:竞价+一口价',
    `start_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '开始时间',
    `end_time` DATETIME NOT NULL COMMENT '结束时间',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0:进行中,1:已售出,2:已下架,3:已过期',
    `listing_fee` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '上架费',
    `sold_fee_rate` DECIMAL(4,3) NOT NULL DEFAULT 0.050 COMMENT '成交手续费率',
    PRIMARY KEY (`id`),
    KEY `idx_seller` (`seller_id`),
    KEY `idx_item_id` (`item_id`),
    KEY `idx_end_time` (`end_time`, `status`),
    KEY `idx_status` (`status`),
    KEY `idx_auction_type` (`auction_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='拍卖行表';

-- 竞价历史表
CREATE TABLE `auction_bid_history` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `auction_id` BIGINT UNSIGNED NOT NULL COMMENT '拍卖ID',
    `bidder_id` BIGINT UNSIGNED NOT NULL COMMENT '出价者ID',
    `bidder_name` VARCHAR(64) NOT NULL COMMENT '出价者名称',
    `bid_amount` INT UNSIGNED NOT NULL COMMENT '出价金额',
    `bid_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '出价时间',
    PRIMARY KEY (`id`),
    KEY `idx_auction_id` (`auction_id`),
    KEY `idx_bidder_id` (`bidder_id`),
    KEY `idx_bid_time` (`bid_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='竞价历史表';
```

### 2.2 数据结构

```cpp
// 拍卖行数据结构

enum class AuctionStatus {
    ACTIVE = 0,          // 进行中
    SOLD = 1,            // 已售出
    CANCELLED = 2,       // 已下架
    EXPIRED = 3,         // 已过期
};

enum class AuctionType {
    BID = 0,             // 仅竞价
    BUYOUT = 1,          // 仅一口价
    BID_AND_BUYOUT = 2,  // 竞价+一口价
};

// 拍卖物品
struct AuctionItem {
    uint64_t auctionId;
    uint64_t sellerId;
    std::string sellerName;
    int itemId;                  // 物品模板ID
    uint64_t itemUniqueId;       // 唯一物品ID (装备)
    int itemCount;
    int itemQuality;
    int enchantLevel;

    uint32_t startPrice;
    uint32_t buyoutPrice;        // NULL 表示无一口价
    uint32_t currentBid;
    uint64_t currentBidderId;
    std::string currentBidderName;
    uint32_t minBidIncrement;

    AuctionType auctionType;
    uint64_t startTime;
    uint64_t endTime;
    AuctionStatus status;

    uint32_t listingFee;         // 上架费
    float soldFeeRate;           // 成交手续费率
};

// 搜索条件
struct AuctionSearchCriteria {
    int itemId = 0;              // 物品ID，0表示所有
    int minLevel = 0;            // 最低等级要求
    int maxLevel = 0;            // 最高等级要求
    int minQuality = 0;          // 最低品质
    int maxQuality = 0;          // 最高品质
    uint32_t minPrice = 0;       // 最低价格
    uint32_t maxPrice = 0;       // 最高价格
    std::string nameFilter;      // 名称搜索

    enum SortBy {
        PRICE_ASC,
        PRICE_DESC,
        TIME_LEFT,
        QUALITY_DESC,
        LEVEL_ASC,
    };
    SortBy sortBy = PRICE_ASC;

    int page = 0;
    int pageSize = 50;
};
```

---

## 三、拍卖行系统实现

### 3.1 拍卖行管理器

```cpp
// 拍卖行系统

class AuctionHouseSystem {
public:
    static constexpr uint32_t DEFAULT_AUCTION_DURATION = 24 * 3600; // 24小时
    static constexpr float DEFAULT_LISTING_FEE_RATE = 0.01f;       // 1% 上架费
    static constexpr float DEFAULT_SOLD_FEE_RATE = 0.05f;          // 5% 成交费
    static constexpr uint32_t MIN_BID_INCREMENT_PERCENTAGE = 5;    // 最小加价5%

    // 上架物品
    uint64_t listItem(uint64_t sellerId, const AuctionItem& item) {
        // 1. 检查卖家条件
        if (!canList(sellerId, item)) {
            return 0;
        }

        // 2. 计算上架费
        uint32_t listingFee = calculateListingFee(item);
        if (!chargeListingFee(sellerId, listingFee)) {
            return 0;
        }

        // 3. 从背包移除物品
        if (!removeItemFromBag(sellerId, item)) {
            refundListingFee(sellerId, listingFee);
            return 0;
        }

        // 4. 创建拍卖
        uint64_t auctionId = createAuction(item);
        if (auctionId == 0) {
            // 失败退还物品和费用
            addItemToBag(sellerId, item);
            refundListingFee(sellerId, listingFee);
            return 0;
        }

        // 5. 添加到索引
        addToIndex(auctionId, item);

        // 6. 设置过期定时器
        scheduleAuctionExpiry(auctionId, item.endTime);

        // 7. 通知客户端
        notifyAuctionListed(sellerId, auctionId);

        return auctionId;
    }

    // 竞价
    bool placeBid(uint64_t auctionId, uint64_t bidderId, uint32_t bidAmount) {
        // 1. 获取拍卖信息
        AuctionItem* auction = getAuction(auctionId);
        if (!auction || auction->status != AuctionStatus::ACTIVE) {
            return false;
        }

        // 2. 检查拍卖类型
        if (auction->auctionType == AuctionType::BUYOUT) {
            return false;  // 一口价拍卖不能竞价
        }

        // 3. 检查是否已过期
        if (getCurrentTime() >= auction->endTime) {
            return false;
        }

        // 4. 检查出价是否有效
        uint32_t minBid = auction->currentBid + auction->minBidIncrement;
        if (bidAmount < minBid) {
            sendError(bidderId, "Bid too low");
            return false;
        }

        // 5. 检查玩家金币
        auto* entity = getEntity(bidderId);
        if (!entity || entity->getGold() < bidAmount) {
            sendError(bidderId, "Not enough gold");
            return false;
        }

        // 6. 扣除出价
        entity->addGold(-static_cast<int>(bidAmount));

        // 7. 退还之前出价者的金币
        if (auction->currentBidderId != 0) {
            refundBid(auction->currentBidderId, auction->currentBid);
            notifyOutbid(auction->currentBidderId, auctionId);
        }

        // 8. 更新拍卖
        uint32_t oldBid = auction->currentBid;
        auction->currentBid = bidAmount;
        auction->currentBidderId = bidderId;
        auction->currentBidderName = getPlayerName(bidderId);

        // 9. 记录竞价历史
        recordBidHistory(auctionId, bidderId, bidAmount);

        // 10. 更新数据库
        database_->execute(fmt::format(
            "UPDATE auction_house SET "
            "current_bid = {}, current_bidder_id = '{}', current_bidder_name = '{}' "
            "WHERE id = {}",
            bidAmount, bidderId, auction->currentBidderName, auctionId
        ));

        // 11. 通知
        notifyBidPlaced(auctionId, bidderId, bidAmount);
        updateAuctionInSearch(auctionId);

        return true;
    }

    // 一口价购买
    bool buyout(uint64_t auctionId, uint64_t buyerId) {
        // 1. 获取拍卖信息
        AuctionItem* auction = getAuction(auctionId);
        if (!auction || auction->status != AuctionStatus::ACTIVE) {
            return false;
        }

        // 2. 检查是否有一口价
        if (auction->buyoutPrice == 0) {
            return false;
        }

        // 3. 检查是否卖家自己购买
        if (auction->sellerId == buyerId) {
            return false;
        }

        // 4. 检查玩家金币
        auto* entity = getEntity(buyerId);
        if (!entity || entity->getGold() < auction->buyoutPrice) {
            sendError(buyerId, "Not enough gold");
            return false;
        }

        // 5. 扣除金币
        entity->addGold(-static_cast<int>(auction->buyoutPrice));

        // 6. 退还之前出价者的金币 (如果有)
        if (auction->currentBidderId != 0) {
            refundBid(auction->currentBidderId, auction->currentBid);
        }

        // 7. 计算手续费
        uint32_t fee = static_cast<uint32_t>(auction->buyoutPrice * auction->soldFeeRate);
        uint32_t sellerReceive = auction->buyoutPrice - fee;

        // 8. 给卖家金币
        addGold(auction->sellerId, sellerReceive);

        // 9. 给买家物品
        giveItemToBuyer(buyerId, *auction);

        // 10. 更新拍卖状态
        auction->status = AuctionStatus::SOLD;
        database_->execute(fmt::format(
            "UPDATE auction_house SET status = 1 WHERE id = {}",
            auctionId
        ));

        // 11. 移除定时器
        cancelExpiryTimer(auctionId);

        // 12. 从索引移除
        removeFromIndex(auctionId);

        // 13. 通知
        notifyAuctionSold(auctionId, buyerId);
        sendAuctionResultMail(auction->sellerId, *auction, sellerReceive);
        sendBuyoutSuccessMail(buyerId, *auction);

        return true;
    }

    // 取消拍卖
    bool cancelAuction(uint64_t auctionId, uint64_t sellerId) {
        AuctionItem* auction = getAuction(auctionId);
        if (!auction) return false;

        // 检查权限
        if (auction->sellerId != sellerId) {
            return false;
        }

        // 检查状态
        if (auction->status != AuctionStatus::ACTIVE) {
            return false;
        }

        // 检查是否有人出价
        if (auction->currentBidderId != 0) {
            // 有出价不能取消
            sendError(sellerId, "Cannot cancel auction with active bids");
            return false;
        }

        // 更新状态
        auction->status = AuctionStatus::CANCELLED;
        database_->execute(fmt::format(
            "UPDATE auction_house SET status = 2 WHERE id = {}",
            auctionId
        ));

        // 返还物品
        returnItemToSeller(sellerId, *auction);

        // 移除定时器
        cancelExpiryTimer(auctionId);

        // 从索引移除
        removeFromIndex(auctionId);

        notifyAuctionCancelled(auctionId);
        return true;
    }

    // 搜索拍卖
    std::vector<AuctionItem> search(const AuctionSearchCriteria& criteria) {
        std::vector<AuctionItem> results;

        // 构建 SQL 查询
        std::string sql = "SELECT * FROM auction_house WHERE status = 0";
        std::vector<std::string> conditions;

        if (criteria.itemId > 0) {
            conditions.push_back(fmt::format("item_id = {}", criteria.itemId));
        }
        if (criteria.minQuality > 0) {
            conditions.push_back(fmt::format("item_quality >= {}", criteria.minQuality));
        }
        if (criteria.maxQuality > 0) {
            conditions.push_back(fmt::format("item_quality <= {}", criteria.maxQuality));
        }
        if (criteria.minPrice > 0) {
            conditions.push_back(fmt::format("IFNULL(buyout_price, current_bid) >= {}", criteria.minPrice));
        }
        if (criteria.maxPrice > 0) {
            conditions.push_back(fmt::format("IFNULL(buyout_price, current_bid) <= {}", criteria.maxPrice));
        }
        if (!criteria.nameFilter.empty()) {
            conditions.push_back(fmt::format("item_name LIKE '%{}%'", criteria.nameFilter));
        }

        for (const auto& cond : conditions) {
            sql += " AND " + cond;
        }

        // 排序
        switch (criteria.sortBy) {
            case AuctionSearchCriteria::PRICE_ASC:
                sql += " ORDER BY IFNULL(buyout_price, current_bid) ASC";
                break;
            case AuctionSearchCriteria::PRICE_DESC:
                sql += " ORDER BY IFNULL(buyout_price, current_bid) DESC";
                break;
            case AuctionSearchCriteria::TIME_LEFT:
                sql += " ORDER BY end_time ASC";
                break;
            case AuctionSearchCriteria::QUALITY_DESC:
                sql += " ORDER BY item_quality DESC";
                break;
            case AuctionSearchCriteria::LEVEL_ASC:
                sql += " ORDER BY item_required_level ASC";
                break;
        }

        // 分页
        sql += fmt::format(" LIMIT {} OFFSET {}",
                          criteria.pageSize,
                          criteria.page * criteria.pageSize);

        // 执行查询
        auto queryResults = database_->query(sql);

        for (const auto& row : queryResults) {
            results.push_back(parseAuctionRow(row));
        }

        return results;
    }

    // 获取卖家拍卖列表
    std::vector<AuctionItem> getSellerAuctions(uint64_t sellerId) {
        std::vector<AuctionItem> results;

        auto queryResults = database_->query(fmt::format(
            "SELECT * FROM auction_house WHERE seller_id = {} AND status = 0",
            sellerId
        ));

        for (const auto& row : queryResults) {
            results.push_back(parseAuctionRow(row));
        }

        return results;
    }

private:
    bool canList(uint64_t sellerId, const AuctionItem& item) {
        // 检查玩家状态
        auto* entity = getEntity(sellerId);
        if (!entity) return false;

        // 检查物品是否可交易
        if (!isItemTradeable(sellerId, item.itemUniqueId)) {
            return false;
        }

        // 检查价格
        if (item.startPrice == 0 && item.buyoutPrice == 0) {
            return false;
        }

        // 检查拍卖数量限制
        int activeCount = countActiveAuctions(sellerId);
        int maxAuctions = getMaxAuctions(sellerId);
        if (activeCount >= maxAuctions) {
            sendError(sellerId, "Maximum auction limit reached");
            return false;
        }

        return true;
    }

    uint32_t calculateListingFee(const AuctionItem& item) {
        // 上架费 = 一口价或起始价的 1%
        uint32_t basePrice = item.buyoutPrice > 0 ? item.buyoutPrice : item.startPrice;
        return static_cast<uint32_t>(basePrice * DEFAULT_LISTING_FEE_RATE);
    }

    bool chargeListingFee(uint64_t sellerId, uint32_t fee) {
        auto* entity = getEntity(sellerId);
        if (!entity || entity->getGold() < fee) {
            return false;
        }
        entity->addGold(-static_cast<int>(fee));
        return true;
    }

    void refundListingFee(uint64_t sellerId, uint32_t fee) {
        addGold(sellerId, fee);
    }

    bool removeItemFromBag(uint64_t sellerId, const AuctionItem& item) {
        // 从背包移除物品
        return inventory_->removeItem(sellerId, item.itemUniqueId, item.itemCount);
    }

    void addItemToBag(uint64_t sellerId, const AuctionItem& item) {
        // 退还物品
        inventory_->addItem(sellerId, item.itemId, item.itemCount);
    }

    uint64_t createAuction(const AuctionItem& item) {
        database_->execute(fmt::format(
            "INSERT INTO auction_house "
            "(seller_id, seller_name, item_id, item_unique_id, item_count, item_quality, "
            "start_price, buyout_price, min_bid_increment, auction_type, end_time, "
            "listing_fee, sold_fee_rate) "
            "VALUES ({}, '{}', {}, {}, {}, {}, {}, {}, {}, {}, FROM_UNIXTIME({}), {}, {})",
            item.sellerId, item.sellerName, item.itemId, item.itemUniqueId,
            item.itemCount, item.itemQuality, item.startPrice,
            item.buyoutPrice, item.minBidIncrement,
            static_cast<int>(item.auctionType), item.endTime,
            item.listingFee, item.soldFeeRate
        ));

        return database_->lastInsertId();
    }

    void refundBid(uint64_t bidderId, uint32_t amount) {
        addGold(bidderId, amount);

        // 发送通知
        sendToClient(bidderId, "onBidRefunded", amount);
    }

    void giveItemToBuyer(uint64_t buyerId, const AuctionItem& auction) {
        inventory_->addItem(buyerId, auction.itemId, auction.itemCount);

        // 如果是唯一物品 (装备)，需要转移唯一ID
        if (auction.itemUniqueId != 0) {
            database_->execute(fmt::format(
                "UPDATE player_bag SET player_id = {} WHERE id = {}",
                buyerId, auction.itemUniqueId
            ));
        }
    }

    void returnItemToSeller(uint64_t sellerId, const AuctionItem& auction) {
        inventory_->addItem(sellerId, auction.itemId, auction.itemCount);

        if (auction.itemUniqueId != 0) {
            database_->execute(fmt::format(
                "UPDATE player_bag SET player_id = {} WHERE id = {}",
                sellerId, auction.itemUniqueId
            ));
        }
    }

    void recordBidHistory(uint64_t auctionId, uint64_t bidderId, uint32_t amount) {
        database_->execute(fmt::format(
            "INSERT INTO auction_bid_history "
            "(auction_id, bidder_id, bidder_name, bid_amount) "
            "VALUES ({}, {}, '{}', {})",
            auctionId, bidderId, getPlayerName(bidderId), amount
        ));
    }

    void sendAuctionResultMail(uint64_t playerId, const AuctionItem& auction,
                              uint32_t goldReceived) {
        mailSystem_->sendMail(playerId, "Auction Sold",
                            fmt::format("Your auction sold for {} gold.",
                                       auction.currentBid),
                            {}, goldReceived);
    }

    void sendBuyoutSuccessMail(uint64_t playerId, const AuctionItem& auction) {
        // 一口价成功通知
    }

    void sendAuctionExpiredMail(uint64_t sellerId, const AuctionItem& auction) {
        if (auction.currentBidderId != 0) {
            // 有人出价但过期，物品给出价者
            giveItemToBuyer(auction.currentBidderId, auction);
            uint32_t fee = static_cast<uint32_t>(auction.currentBid * auction.soldFeeRate);
            uint32_t sellerReceive = auction.currentBid - fee;
            addGold(sellerId, sellerReceive);

            mailSystem_->sendMail(auction.currentBidderId, "Auction Won",
                                "You won the auction!",
                                {{auction.itemId, auction.itemCount}}, 0);
        } else {
            // 没人出价，退还物品
            returnItemToSeller(sellerId, auction);
            mailSystem_->sendMail(sellerId, "Auction Expired",
                                "Your auction expired without bids.",
                                {{auction.itemId, auction.itemCount}}, 0);
        }
    }

    void processExpiredAuction(uint64_t auctionId) {
        AuctionItem* auction = getAuction(auctionId);
        if (!auction || auction->status != AuctionStatus::ACTIVE) {
            return;
        }

        auction->status = AuctionStatus::EXPIRED;
        database_->execute(fmt::format(
            "UPDATE auction_house SET status = 3 WHERE id = {}",
            auctionId
        ));

        sendAuctionExpiredMail(auction->sellerId, *auction);
        removeFromIndex(auctionId);
    }

    int countActiveAuctions(uint64_t sellerId) {
        auto result = database_->query(fmt::format(
            "SELECT COUNT(*) as cnt FROM auction_house WHERE seller_id = {} AND status = 0",
            sellerId
        ));
        return result[0]["cnt"].get<int>();
    }

    int getMaxAuctions(uint64_t sellerId) {
        // VIP 可以更多拍卖
        auto* entity = getEntity(sellerId);
        if (entity) {
            int vipLevel = entity->getVipLevel();
            return 10 + vipLevel * 5;
        }
        return 10;  // 默认10个
    }

    std::unordered_map<uint64_t, AuctionItem> auctions_;
};
```

---

## 四、Redis 缓存优化

### 4.1 拍卖行缓存

```cpp
// Redis 缓存的拍卖行

class CachedAuctionHouse : public AuctionHouseSystem {
public:
    // 使用 Sorted Set 存储拍卖列表，按价格排序
    std::vector<AuctionItem> searchCached(const AuctionSearchCriteria& criteria) {
        std::string cacheKey = buildCacheKey(criteria);

        // 尝试从缓存获取
        std::string cached = redis_->get(cacheKey);
        if (!cached.empty()) {
            return deserializeResults(cached);
        }

        // 缓存未命中，查询数据库
        auto results = search(criteria);

        // 写入缓存，5分钟过期
        redis_->setex(cacheKey, 300, serializeResults(results));

        return results;
    }

    // 更新拍卖后使缓存失效
    void invalidateCache(int itemId) {
        std::string pattern = fmt::format("auction:item_{}:*", itemId);
        redis_->delPattern(pattern);
    }

private:
    std::string buildCacheKey(const AuctionSearchCriteria& criteria) {
        return fmt::format("auction:item_{}:qmin_{}:qmax_{}:pmin_{}:pmax_{}:sort_{}",
                          criteria.itemId,
                          criteria.minQuality,
                          criteria.maxQuality,
                          criteria.minPrice,
                          criteria.maxPrice,
                          static_cast<int>(criteria.sortBy));
    }
};
```

---

## 五、最佳实践

### 5.1 拍卖行设计建议

| 实践 | 说明 |
|------|------|
| **Redis 缓存** | 热门物品列表缓存 |
| **分页加载** | 减少单次查询数据量 |
| **异步处理** | 过期拍卖异步处理 |
| **搜索优化** | 建立合适的索引 |
| **防刷机制** | 限制上架数量和频率 |

### 5.2 性能优化

```
优化策略:
1. Redis 缓存热门搜索
2. 数据库读写分离
3. 定期归档过期拍卖
4. 使用消息队列处理通知
5. 分表存储历史记录
```

---

## 六、总结

### 拍卖行系统核心

```
拍卖行 = 上架系统 + 竞价机制 + 搜索系统 + 过期处理
- 上架时锁定物品和收取费用
- 竞价时退款机制
- 缓存优化搜索性能
- 异步处理过期拍卖
```

---

## 参考资料

- [魔兽世界拍卖行设计](https://www.gamedev.net/)
- [电商秒杀系统设计](https://tech.meituan.com/)
