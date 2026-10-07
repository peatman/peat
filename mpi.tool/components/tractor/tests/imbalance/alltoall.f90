!
! This file is part of PEAT (Performance Estimation & Analysing Toolkit).
! Copyright (C) 2026 by Alex Knyazev
!
! PEAT is free software: you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation, either version 3 of the License, or
! (at your option) any later version.
!
! PEAT is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License
! along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
!

! NOTE: this file relies on the C preprocessor (cpp) for the READ_IDLEVAL
! macro below; compile with -cpp (gfortran) or -fpp (ifx/ifort).
! #define READ_IDLEVAL /* Intel MPI specific !!! */

program alltoall
    use mpi_f08
    use, intrinsic :: iso_c_binding, only: c_int
    implicit none

    ! MPI_T tool interface has no Fortran library binding in this MPI
    ! implementation, so call the C entry points directly.
    interface
        function MPI_T_init_thread_c(required, provided) result(res) bind(C, name="MPI_T_init_thread")
            import :: c_int
            integer(c_int), value :: required
            integer(c_int) :: provided
            integer(c_int) :: res
        end function MPI_T_init_thread_c
        function MPI_T_finalize_c() result(res) bind(C, name="MPI_T_finalize")
            import :: c_int
            integer(c_int) :: res
        end function MPI_T_finalize_c
    end interface

#ifdef READ_IDLEVAL
    type(MPI_T_pvar_session) :: session_idle
    type(MPI_T_pvar_handle)  :: handle_idle
#endif
    integer(c_int) :: provided_c, mpit_ierr
    integer :: rank, wsize, nnodes, ppn, buf_size, tot_it
    integer :: n, p, r, c, count, ierr
    double precision :: val_after, val_before, val
    double precision :: idle_val_after, idle_val_before, idle_val
    integer, allocatable :: ranks_comms(:,:)
    type(MPI_Group), allocatable :: groups_comms(:)
    type(MPI_Group) :: groups_world
    type(MPI_Comm), allocatable :: comms(:)
    integer(kind=1), allocatable :: sendbuf(:), recvbuf(:)
    character(len=32) :: arg

#ifdef READ_IDLEVAL
    mpit_ierr = MPI_T_init_thread_c(int(MPI_THREAD_SINGLE, c_int), provided_c)
#endif
    call MPI_Init(ierr)
    ! let make it easier and read number of nodes and ppn from command line
#ifdef READ_IDLEVAL
    call MPI_T_pvar_session_create(session_idle, ierr)
    count = 0
    call MPI_T_pvar_handle_alloc(session_idle, 0, MPI_T_PVAR_ALL_HANDLES, handle_idle, count, ierr) ! 0 - sec 1 - ticks?
#endif

    val_after = 0.0d0
    val_before = 0.0d0
    val = 0.0d0
    idle_val_after = 0.0d0
    idle_val_before = 0.0d0
    idle_val = 0.0d0

    if (command_argument_count() >= 4) then
        call MPI_Comm_size(MPI_COMM_WORLD, wsize, ierr)
        call MPI_Comm_rank(MPI_COMM_WORLD, rank, ierr)

        call get_command_argument(1, arg)
        read(arg, *) nnodes
        call get_command_argument(2, arg)
        read(arg, *) ppn
        call get_command_argument(3, arg)
        read(arg, *) buf_size
        call get_command_argument(4, arg)
        read(arg, *) tot_it

        if (rank == 0) then
            print '(A,I0,A,I0,A,I0,A,I0)', &
                'nnodes=', nnodes, ',ppn=', ppn, ',buf_size=', buf_size, ',iterations=', tot_it
        end if

        ! create some comms 1 rank from each node
        allocate(ranks_comms(nnodes, ppn))
        allocate(groups_comms(ppn))
        allocate(comms(ppn))

        r = 0
        call MPI_Comm_group(MPI_COMM_WORLD, groups_world, ierr)
        do n = 1, nnodes
            do p = 1, ppn
                ranks_comms(n, p) = r
                r = r + 1
            end do
        end do

        do p = 1, ppn
            call MPI_Group_incl(groups_world, nnodes, ranks_comms(:, p), groups_comms(p), ierr)
            call MPI_Comm_create(MPI_COMM_WORLD, groups_comms(p), comms(p), ierr)
            if (rank == 0) then
                write(*, '(A)', advance='no') 'Comm Created. Ranks:'
                do n = 1, nnodes
                    write(*, '(I0,A)', advance='no') ranks_comms(n, p), ' '
                end do
                write(*, *)
            end if
        end do

        allocate(sendbuf(buf_size * nnodes))
        allocate(recvbuf(buf_size * nnodes))

        call MPI_Barrier(MPI_COMM_WORLD, ierr)
        do c = 1, tot_it
            val_before = MPI_Wtime()
#ifdef READ_IDLEVAL
            call MPI_T_pvar_read(session_idle, handle_idle, idle_val_before, ierr)
#endif
            call MPI_Alltoall(sendbuf, buf_size, MPI_BYTE, recvbuf, buf_size, MPI_BYTE, comms(mod(rank, ppn) + 1), ierr)
#ifdef READ_IDLEVAL
            call MPI_T_pvar_read(session_idle, handle_idle, idle_val_after, ierr)
#endif
            val_after = MPI_Wtime()
            idle_val = idle_val + (idle_val_after - idle_val_before)
            val = val + (val_after - val_before)
            call MPI_Barrier(MPI_COMM_WORLD, ierr)
        end do
        print '(A,F0.2,A,F0.2)', 'Total time ', val, ', Idle time ', idle_val

        deallocate(sendbuf)
        deallocate(recvbuf)
        deallocate(ranks_comms)
        deallocate(groups_comms)
        deallocate(comms)
    end if

#ifdef READ_IDLEVAL
    call MPI_T_pvar_handle_free(session_idle, handle_idle, ierr)
    call MPI_T_pvar_session_free(session_idle, ierr)
    mpit_ierr = MPI_T_finalize_c()
#endif

    call MPI_Finalize(ierr)

end program alltoall
