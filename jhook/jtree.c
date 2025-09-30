/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stddef.h>
#include "jtree.h"

#define JRBTREE_RED             0
#define JRBTREE_BLACK           1

#define rb_parent(r)            ((struct jtree *)((r)->parent_color & ~((uintptr_t)3)))
#define rb_color(r)             ((r)->parent_color & (uintptr_t)1)
#define rb_is_red(r)            (!rb_color(r))
#define rb_is_black(r)          rb_color(r)

#define rb_set_parent(r, p)     do { (r)->parent_color = ((r)->parent_color &  (uintptr_t)1) | (uintptr_t)(p); } while (0)
#define rb_set_color(r, c)      do { (r)->parent_color = ((r)->parent_color & ~((uintptr_t)1)) | c; } while (0)
#define rb_set_red(r)           do { (r)->parent_color &= ~((uintptr_t)1); } while (0)
#define rb_set_black(r)         do { (r)->parent_color |=  (uintptr_t)1; } while (0)

static void jtree_rotate_left(struct jtree_root *root, struct jtree *node)
{
    struct jtree *right = node->right_son;        // N的右节点R
    struct jtree *parent = rb_parent(node);       // N的父节点P

    /*
     * 左旋示意图(对节点N进行左旋)
     *   P                 P
     *   |                 |
     *   N                 R
     *  / \  --(左旋)-->  / \
     * L   R             N  RR
     *    / \           / \
     *   LR RR         L  LR
     */
    if ((node->right_son = right->left_son))        // 将LR设为N的右节点
        rb_set_parent(right->left_son, node);       // LR不为空时将N设为LR的父节点
    right->left_son = node;                         // 将N设为R的左节点
    rb_set_parent(node, right);                     // 将R设为N的父节点

    rb_set_parent(right, parent);                   // 将P设为R的父节点
    if (parent) {                                   // P不为空时将R设为P对应的左或右节点
        if (node == parent->left_son)
            parent->left_son = right;
        else
            parent->right_son = right;
    } else {                                        // P为空时将R设为根节点
        root->head = right;
    }
}

static void jtree_rotate_right(struct jtree_root *root, struct jtree *node)
{
    struct jtree *left = node->left_son;          // N的左节点L
    struct jtree *parent = rb_parent(node);       // N的父节点P

    /*
     * 右旋示意图(对节点N进行右旋)
     *     P                 P
     *     |                 |
     *     N                 L
     *    / \  --(右旋)-->  / \
     *   L   R             LL  N
     *  / \                   / \
     * LL RL                 RL  R
     */
    if ((node->left_son = left->right_son))         // 将RL设为N的左节点
        rb_set_parent(left->right_son, node);       // RL不为空时将N设为RL的父节点
    left->right_son = node;                         // 将N设为L的右节点
    rb_set_parent(node, left);                      // 将L设为N的父节点

    rb_set_parent(left, parent);                    // 将P设为L的父节点
    if (parent) {                                   // P不为空时将L设为P对应的左或右节点
        if (node == parent->right_son)
            parent->right_son = left;
        else
            parent->left_son = left;
    } else {                                        // P为空时将L设为根节点
        root->head = left;
    }
}

static void jtree_add_fixup(struct jtree_root *root, struct jtree *node)
{
    struct jtree *parent, *gparent, *uncle, *tmp;

    /*
     * 插入的节点的默认颜色是红色，插入情况分为3种：
     * 1. 被插入的节点是根节点(该节点的父节点为NULL)，违反第2条，处理方法是直接把此节点涂为黑色
     * 2. 被插入的节点的父节点是黑色，不违反任何规则，不需要做任何处理
     * 3. 被插入的节点的父节点是红色，违反第4条，处理的核心思想是将红色上移到根节点，然后，将根节点设为黑色
     * 第3条又可以分为3x2种情况，见如下处理措施
     */
    while ((parent = rb_parent(node)) && rb_is_red(parent)) {
        gparent = rb_parent(parent);

        if (parent == gparent->left_son) {          // 父节点是祖父节点的左节点
            uncle = gparent->right_son;
            if (uncle && rb_is_red(uncle)) {        // 情况3.1.1：叔节点是红色
                /*
                 *    bG                  rG
                 *    / \                 / \
                 *   rP rU  --(变色)-->  bP bU
                 *  /                   /
                 * rN                  rN
                 */
                rb_set_black(parent);
                rb_set_black(uncle);
                rb_set_red(gparent);
                node = gparent;
                continue;
            }

            if (parent->right_son == node) {        // 情况3.1.2：叔节点是黑色，且当前节点是右节点
                /*
                 *  bG                  bG
                 *  / \                 / \
                 * rP bU  --(左旋)-->  rN bU
                 *  \                  /
                 *  rN                rP
                 */
                jtree_rotate_left(root, parent);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            /*
             *    bG                      bP
             *    / \                     / \
             *   rP bU  --(变色右旋)-->  rN rG
             *  /                            \
             * rN                            bU
             */
            rb_set_black(parent);                   // 情况3.1.3：叔节点是黑色，且当前节点是左节点
            rb_set_red(gparent);
            jtree_rotate_right(root, gparent);
        } else {                                    // 父节点是祖父节点的右节点
            uncle = gparent->left_son;
            if (uncle && rb_is_red(uncle)) {        // 情况3.2.1：叔节点是红色
                /*
                 *  bG                  rG
                 *  / \                 / \
                 * rU rP  --(变色)-->  bU bP
                 *     \                   \
                 *     rN                  rN
                 */
                rb_set_black(parent);
                rb_set_black(uncle);
                rb_set_red(gparent);
                node = gparent;
                continue;
            }

            if (parent->left_son == node) {         // 情况3.2.2：叔节点是黑色，且当前节点是左节点
                /*
                 *  bG                  bG
                 *  / \                 / \
                 * bU rP  --(右旋)-->  bU rN
                 *    /                    \
                 *   rN                    rP
                 */
                jtree_rotate_right(root, parent);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            /*
             *    bG                      bP
             *    / \                     / \
             *   bU rP  --(变色左旋)-->  rG rN
             *       \                   /
             *       rN                 bU
             */
            rb_set_black(parent);                   // 情况3.2.3：叔节点是黑色，且当前节点是右节点
            rb_set_red(gparent);
            jtree_rotate_left(root, gparent);
        }
    }

    rb_set_black(root->head);                       // 将根节点涂为黑色
}

int jtree_add(struct jtree_root *root, struct jtree *node)
{
    struct jtree **pn = &root->head;
    struct jtree *parent = NULL;
    int result = 0;

    if (!root->node_cmp)
        return -1;

    while (*pn) {
        parent = *pn;
        result = root->node_cmp(parent, node);
        if (result > 0)
            pn = &parent->left_son;
        else if (result < 0)
            pn = &parent->right_son;
        else
            return -1;
    }

    node->parent_color = (uintptr_t)parent;         // 设置新增节点的父节点，新节点的默认颜色为红色
    node->left_son = NULL;
    node->right_son = NULL;
    *pn = node;                                     // 设置父节点的左或右节点为新节点
    jtree_add_fixup(root, node);
    ++root->num;

    return 0;
}

static void jtree_del_fixup(struct jtree_root *root, struct jtree *node, struct jtree *parent)
{
    struct jtree *brother;

    /*
     * 如果删除的是黑节点，需要进行平衡操作，处理的核心思想是将黑色上移到x，分3种情况：
     * 1. x是红节点，处理方法是直接把x涂为黑色
     * 2. x是黑节点且为根节点，不需要做任何处理
     * 3. x是黑节点且不为根节点，又分为下面2x4情况处理，直到满足1或2再处理
     */
    while ((!node || rb_is_black(node)) && node != root->head) {
        if (parent->left_son == node) {             // 测试节点是父节点的左节点
            brother = parent->right_son;
            if (rb_is_red(brother)) {
                /*
                 * 情况3.1.1：兄弟节点是红色
                 *    bP                      bB
                 *    / \                     / \
                 *   bN rB  --(变色左旋)-->  rP bR
                 *      / \                  / \
                 *     bL bR                bN bL
                 */
                rb_set_black(brother);
                rb_set_red(parent);
                jtree_rotate_left(root, parent);
                brother = parent->right_son;
            }

            if ((!brother->left_son || rb_is_black(brother->left_son)) &&
                (!brother->right_son || rb_is_black(brother->right_son))) {
                /*
                 * 情况3.1.2：兄弟节点为黑色，且兄弟节点的子节点都为黑色
                 *    ?P                  ?P
                 *    / \                 / \
                 *   bN bB  --(变色)-->  bN rB
                 *      / \                 / \
                 *     bL bR               bL bR
                 */
                rb_set_red(brother);
                node = parent;
                parent = rb_parent(node);
            } else {
                if (!brother->right_son || rb_is_black(brother->right_son)) {
                    /*
                     * 情况3.1.3：兄弟节点为黑色，且兄弟节点的左节点为红色右节点为黑色
                     *    ?P                      ?P
                     *    / \                     / \
                     *   bN bB  --(变色右旋)-->  bN bL
                     *      / \                     / \
                     *     rL bR                   bX rB
                     *    / \                        / \
                     *   bX bY                      bY bR
                     */
                    rb_set_black(brother->left_son);
                    rb_set_red(brother);
                    jtree_rotate_right(root, brother);
                    brother = parent->right_son;
                }

                /*
                 * 情况3.1.4：兄弟节点为黑色，且兄弟节点的左节点为任意色右节点为红色
                 *    ?P                       ?B
                 *    / \                     /   \
                 *   bN bB  --(变色左旋)-->  bP   bR
                 *      / \                 / \   / \
                 *     ?L rR               bN ?L bX bY
                 *        / \
                 *       bX bY
                 */
                rb_set_color(brother, rb_color(parent));
                rb_set_black(parent);
                rb_set_black(brother->right_son);
                jtree_rotate_left(root, parent);
                node = root->head;
                break;
            }
        } else {                                    // 测试节点是父节点的右节点
            brother = parent->left_son;
            if (rb_is_red(brother)) {
                /*
                 * 情况3.2.1：兄弟节点是红色
                 *    bP                      bB
                 *    / \                     / \
                 *   rB bN  --(变色右旋)-->  bL rP
                 *  / \                         / \
                 * bL bR                       bR bN
                 */
                rb_set_black(brother);
                rb_set_red(parent);
                jtree_rotate_right(root, parent);
                brother = parent->left_son;
            }

            if ((!brother->left_son || rb_is_black(brother->left_son)) &&
                (!brother->right_son || rb_is_black(brother->right_son))) {
                /*
                 * 情况3.2.2：兄弟节点为黑色，且兄弟节点的子节点都为黑色
                 *    ?P                  ?P
                 *    / \                 / \
                 *   bB bN  --(变色)-->  bB bN
                 *  / \                 / \
                 * bL bR               bL bR
                 */
                rb_set_red(brother);
                node = parent;
                parent = rb_parent(node);
            } else {
                if (!brother->left_son || rb_is_black(brother->left_son)) {
                    /*
                     * 情况3.2.3：兄弟节点为黑色，且兄弟节点的左节点为黑色右节点为红色
                     *         ?P                      ?P
                     *         / \                     / \
                     *        bB bN  --(变色左旋)-->  bR bN
                     *       / \                     / \
                     *      bL rR                   rB bY
                     *         / \                  / \
                     *        bX bY                bL bX
                     */
                    rb_set_black(brother->right_son);
                    rb_set_red(brother);
                    jtree_rotate_left(root, brother);
                    brother = parent->left_son;
                }

                /*
                 * 情况3.2.4：兄弟节点为黑色，且兄弟节点的左节点为红色右节点为任意色
                 *         ?P                       ?B
                 *         / \                     /   \
                 *        bB bN  --(变色右旋)-->  bL   bP
                 *       / \                     / \   / \
                 *      rL ?R                   bX bY ?R bN
                 *     / \
                 *    bX bY
                 */
                rb_set_color(brother, rb_color(parent));
                rb_set_black(parent);
                rb_set_black(brother->left_son);
                jtree_rotate_right(root, parent);
                node = root->head;
                break;
            }
        }
    }
    if (node)
        rb_set_black(node);
}

void jtree_del(struct jtree_root *root, struct jtree *node)
{
    struct jtree *child, *parent;
    int color;

    /*
     * 删除情况分为3种
     * 1. 被删节点没有子节点，处理方法是直接将该节点删除
     * 2. 被删节点只有1个子节点，处理方法是直接将该节点删除，并用该节点的子节点顶替它的位置
     * 3. 被删节点有2个子节点，处理方法是替换该节点为他右边树最左边的节点
     * 如果删除的是黑节点，需要进行平衡操作
     */
    if (!node->left_son) {
        child = node->right_son;
    } else if (!node->right_son) {
        child = node->left_son;
    } else {
        struct jtree *old = node, *left;

        /*
         * 检索了一级就满足
         *        P                     P
         *        |                     |
         *        N                     B
         *     /     \  --(变换)-->  /     \
         *    A       B             A       c
         *   / \       \           / \
         *  a   b       c         a   b
         *
         * 检索了多级才满足
         *        P                     P
         *        |                     |
         *        N                     c
         *     /     \  --(变换)-->  /     \
         *    A       B             A       B
         *   / \     / \           / \     / \
         *  a   b   c   d         a   b   e   d
         *           \
         *            e
         */
        node = node->right_son;
        while ((left = node->left_son))
            node = left;

        if (rb_parent(old)) {
            if (rb_parent(old)->left_son == old)
                rb_parent(old)->left_son = node;
            else
                rb_parent(old)->right_son = node;
        } else {
            root->head = node;
        }

        child = node->right_son;
        parent = rb_parent(node);
        color = rb_color(node);

        if (parent == old) {
            parent = node;
        } else {
            if (child)
                rb_set_parent(child, parent);
            parent->left_son = child;

            node->right_son = old->right_son;
            rb_set_parent(old->right_son, node);
        }

        node->parent_color = old->parent_color;
        node->left_son = old->left_son;
        rb_set_parent(old->left_son, node);

        goto color;
    }

    /*
     * 只有一个子节点或没有子节点
     *  P               P
     *  |               |
     *  N  --(变换)-->  A
     *  |
     *  A
     */
    parent = rb_parent(node);
    color = rb_color(node);

    if (child)
        rb_set_parent(child, parent);
    if (parent) {
        if (parent->left_son == node)
            parent->left_son = child;
        else
            parent->right_son = child;
    } else {
        root->head = child;
    }

color:
    if (color == JRBTREE_BLACK)
        jtree_del_fixup(root, child, parent);
    --root->num;
}

void jtree_replace(struct jtree_root *root, struct jtree *old, struct jtree *node)
{
    struct jtree *parent = rb_parent(old);

    if (parent) {
        if (old == parent->left_son)
            parent->left_son = node;
        else
            parent->right_son = node;
    } else {
        root->head = node;
    }
    if (old->left_son)
        rb_set_parent(old->left_son, node);
    if (old->right_son)
        rb_set_parent(old->right_son, node);
    *node = *old;
}

struct jtree *jtree_search(struct jtree_root *root, const void *key)
{
    struct jtree *n = root->head;
    int result = 0;

    if (!root->key_cmp)
        return NULL;

    while (n) {
        result = root->key_cmp(n, key);
        if (result > 0)
            n = n->left_son;
        else if (result < 0)
            n = n->right_son;
        else
            return n;
    }
    return NULL;
}

struct jtree *jtree_first(const struct jtree_root *root)
{
    struct jtree *n = root->head;

    if (!n)
        return NULL;
    while (n->left_son)
        n = n->left_son;
    return n;
}

struct jtree *jtree_last(const struct jtree_root *root)
{
    struct jtree *n = root->head;

    if (!n)
        return NULL;
    while (n->right_son)
        n = n->right_son;
    return n;
}

struct jtree *jtree_next(const struct jtree *node)
{
    struct jtree *parent;

    if (node->right_son) {
        node = node->right_son;
        while (node->left_son)
            node = node->left_son;
        return (struct jtree *)node;
    }
    while ((parent = rb_parent(node)) && node == parent->right_son)
        node = parent;

    return parent;
}

struct jtree *jtree_prev(const struct jtree *node)
{
    struct jtree *parent;

    if (node->left_son) {
        node = node->left_son;
        while (node->right_son)
            node = node->right_son;
        return (struct jtree *)node;
    }

    while ((parent = rb_parent(node)) && node == parent->left_son)
        node = parent;

    return parent;
}
