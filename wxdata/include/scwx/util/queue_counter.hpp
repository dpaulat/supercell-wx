#pragma once

#include <memory>
#include <boost/atomic/atomic.hpp>

namespace scwx::util
{

class QueueCounter
{
public:
   /**
    * Counts the number of items in a queue, and prevents it from exceeding a
    * count in a thread safe manor. This is lock free, assuming
    * std::atomic<size_t> supports lock free fetch_add and fetch_sub.
    */

   /**
    * Construct a QueueCounter with a given maximum count
    *
    * @param maxCount The maximum number of items in the queue
    */
   explicit QueueCounter(size_t maxCount);

   ~QueueCounter();
   QueueCounter(const QueueCounter&)            = delete;
   QueueCounter(QueueCounter&&)                 = delete;
   QueueCounter& operator=(const QueueCounter&) = delete;
   QueueCounter& operator=(QueueCounter&&)      = delete;

   /**
    * Called before adding an item. If it returns true, it is ok to add. If it
    * returns false, it should not be added
    *
    * @return true if it is ok to add, false if the queue is full
    */
   bool add();

   /**
    * Called when item is removed from the queue. Should only be called after a
    * corresponding and successful call to add.
    */
   void remove();

   /**
    * Tells if this instance is lock free
    *
    * @return true if it is lock free, false otherwise
    */
   bool is_lock_free();

   /**
    * Tells if this class is always lock free. True if it is lock free, false
    * otherwise
    */
   static constexpr bool is_always_lock_free =
      boost::atomic<size_t>::is_always_lock_free;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::util
