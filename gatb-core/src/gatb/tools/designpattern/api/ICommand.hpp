/*****************************************************************************
 *   GATB : Genome Assembly Tool Box                                         *
 *   Authors: [R.Chikhi, G.Rizk, E.Drezen]                                   *
 *   Based on Minia, Authors: [R.Chikhi, G.Rizk], CeCILL license             *
 *   Copyright (c) INRIA, CeCILL license, 2013                               *
 *****************************************************************************/

/** \file ICommand.hpp
 *  \date 01/03/2013
 *  \author edrezen
 *  \brief Interface for command concept
 */

#ifndef _GATB_CORE_TOOLS_DP_ICOMMAND_HPP_
#define _GATB_CORE_TOOLS_DP_ICOMMAND_HPP_

/********************************************************************************/

#include <gatb/tools/designpattern/api/SmartPointer.hpp>
#include <gatb/tools/designpattern/api/Iterator.hpp>
#include <gatb/system/api/types.hpp>
#include <gatb/system/api/IThread.hpp>

#include <vector>

/********************************************************************************/
namespace gatb  {
namespace core  {
/** \brief Tools package */
namespace tools {
/** \brief Design Patterns concepts */
namespace dp    {
/********************************************************************************/

/** \brief Interface of what a command is.
 *
 *  This class represents the Design Pattern Command whose purpose is to encapsulate
 *  some processing into a uniform calling procedure, a 'execute' method.
 *
 *  The command concept provides a uniform way for running some work.
 *  This is achieved by refining the execute() method of the ICommand interface.
 *  Doing so makes the actual job encapsulated inside the execute() body; clients
 *  that want to use commands only have to know how to launch them: calling execute().
 *
 *  The further step is to introduce an interface that can manage a list of commands.
 *  For instance, in a dual core architecture, it is possible to launch
 *  two instances of commands in two separated threads, which means we got a parallelization
 *  scheme. It is therefore interesting to define an interface that takes a list of commands
 *  an dispatch them in different threads; we call such an interface a command dispatcher.
 *  Note that an implementation of such a dispatcher can parallelize the commands, but another
 *  implementation can only serialize the commands; so, job parallelization or serialization
 *  is just a matter of choice in the actual implementation of the dispatcher interface.
 *
 *  Sample of use:
 * \code
 * class MyCommand : public ICommand
 * {
 * public:
 *     void execute ()  { printf ("I am doing something there\n"); }
 * };
 * \endcode
 *
 * \see ICommandDispatcher
 */
class ICommand : public SmartPointer
{
public:
    /** Method that executes some job. */
    virtual void execute () = 0;
};

/********************************************************************************/

/** \brief Dispatching of commands.
 *
 *  Interface that launches several commands.
 *
 *  According to the implementation, the dispatching can be done in a serial or in
 *  a parallelized way.
 *
 *  Note that a post treatment command can be provided and will be launched when all
 *  the commands have finished.
 *
 *  Note that this interface could also be implemented for dispatching commands over a network in order
 *  to use a grid of computers. We could imagine that the commands dispatching consists in
 *  launching some RCP calls, or creating some web services calls. The important point is that, from the
 *  client point of view, her/his code should not change, except the actual kind of ICommandDispatcher instance
 *  provided to the algorithm.
 *
 *  Sample of use:
 *  \code
 *  // We define a command class
 *  class MyCommand : public ICommand
 *  {
 *  public:
 *      MyCommand (int i) : _i(i) {}
 *      void execute ()  { printf ("Going to sleep %d...\n", _i);  sleep (_i);  }
 *  private:
 *      int _i;
 *  };
 *
 *  int main (int argc, char* argv[])
 *  {
 *      // We create a list of commands
 *      std::list<ICommand*> commands;
 *      commands.push_back (new MyCommand(2));
 *      commands.push_back (new MyCommand(5));
 *      commands.push_back (new MyCommand(3));
 *
 *      // We create a commands dispatcher that parallelizes the execution of the commands.
 *      ICommandDispatcher* dispatcher = new ParallelCommandDispatcher ();
 *
 *      // We launch the 3 commands.
 *      dispatcher->dispatchCommands (commands, 0);
 *
 *      // Here, we should be waiting until the second command (that waits for 5 seconds) is finished.
 *
 *      return 0;
 *  }
 *  \endcode
 *
 *  \see ICommand
 */
class ICommandDispatcher : public SmartPointer
{
public:

    /** Dispatch commands execution in some separate contexts (threads for instance).
     *  Once the commands are launched, this dispatcher waits for the commands finish.
     *  Then, it may have to execute a post treatment command (if any).
     * \param[in] commands      : commands to be executed
     * \param[in] postTreatment : command to be executed after all the commands are done
     */
    virtual void dispatchCommands (std::vector<ICommand*>& commands, ICommand* postTreatment=0) = 0;

    /** Returns the number of execution units for this dispatcher.
     *  For instance, it could be the number of cores in a multi cores architecture.
     *  \return the number of execution units.
     */
    virtual size_t getExecutionUnitsNumber () = 0;

    /** Iterate a provided Iterator instance; each iterated items are processed through some functors.
     *  According to the dispatcher implementation, we can hence iterate in a parallel way on several cores.
     *  Since we can have concurrent access, the iteration is protected by a synchronizer. Note that it is possible
     *  to group items to be given to one functor; grouping may be important for performance issues since we reduce
     *  the number of synchronizer lock/unlock calls and so use in an optimal way the available cores.
     *  \param[in] iterator : the iterator to be iterated
     *  \param[in] functors : vector of functors that are fed with iterated items
     *  \param[in] groupSize : number of items to be retrieved in a single lock/unlock block
     */
    template <typename Item, typename Functor> void iterate (Iterator<Item>* iterator, std::vector<Functor*>& functors, size_t groupSize = 1000)
    {
        /** We create a common synchronizer. */
        system::ISynchronizer* synchro = newSynchro();

        /** We create N IteratorCommand instances. */
        std::vector<ICommand*> commands;
        for (typename std::vector<Functor*>::iterator it = functors.begin(); it != functors.end(); it++)
        {
            commands.push_back (new IteratorCommand<Item,Functor> (iterator, *it, *synchro, groupSize));
        }

        iterator->reset();

        /** We dispatch the commands. */
        dispatchCommands (commands);

        /** We get rid of the synchronizer. */
        delete synchro;
    }

    /** */
    template <typename Item, typename Functor>
    void iterate (Iterator<Item>* iterator, const Functor& functor, size_t groupSize = 1000)
    {
//        /** We create a common synchronizer (to be used for synchronizing shared resources). */
//        system::ISynchronizer* synchro = newSynchro();
//
////        Functor& fct = (Functor&) functor;
////        fct.setSynchro (synchro);

        /** We create N functors that are copies of the provided one. */
        std::vector<Functor*> functors (getExecutionUnitsNumber());
        for (size_t i=0; i<functors.size(); i++)  {  functors[i] = new Functor (functor);  }

        /** We iterate the iterator. */
        iterate (iterator, functors, groupSize);

        /** We get rid of the functors. */
        for (size_t i=0; i<functors.size(); i++)  {  delete functors[i];  }

//        /** We delete the shared synchronizer. */
//        delete synchro;
    }

protected:

    /** Factory method for synchronizer instantiation.
     * \return the created synchronizer. */
    virtual system::ISynchronizer* newSynchro () = 0;

    /** We need some inner class for iterate some iterator in one thread. */
    template <typename Item, typename Functor> class IteratorCommand : public ICommand
    {
    public:
        /** Constructor.
         * \param[in] it : iterator to be used (shared by several IteratorCommand instances)
         * \param[in] fct : functor fed with the iterated items
         * \param[in] synchro : shared synchronizer for accessing several items
         * \param[in] groupSize : number of items got from the iterator in one synchronized block.
         */
        IteratorCommand (Iterator<Item>* it, Functor*& fct, system::ISynchronizer& synchro, size_t groupSize)
            : _it(it), _fct(fct), _synchro(synchro), _groupSize(groupSize)  {}

        /** Implementation of the ICommand interface.*/
        void execute ()
        {
            /** Vector of items to be retrieved from the iterator. */
            std::vector<Item> items (_groupSize);

            /** We begin the iteration. */
            for (bool isRunning=true;  isRunning ; )
            {
                /** We lock the shared synchronizer before accessing the iterator. */
                 _synchro.lock ();

                 /** We retrieve some items from the iterator. */
                 isRunning = _it->get (items);

                 /** We unlock the shared synchronizer after accessing the iterator. */
                 _synchro.unlock ();

                 /** We have retrieved some items from the iterator.
                  * Now, we don't need any more to be synchronized, so we can call the current functor
                  * with the retrieved items. */
                 for (size_t i=0; i<items.size(); i++)  {   (*_fct) (items[i]); }
            }
        }

    private:
        Iterator<Item>*        _it;
        Functor*&              _fct;
        system::ISynchronizer& _synchro;
        size_t                 _groupSize;
    };
};

/********************************************************************************/
} } } } /* end of namespaces. */
/********************************************************************************/

#endif /* _GATB_CORE_TOOLS_DP_ICOMMAND_HPP_ */
