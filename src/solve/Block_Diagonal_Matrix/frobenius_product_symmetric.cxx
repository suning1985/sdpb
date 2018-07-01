#include "../Block_Diagonal_Matrix.hxx"

// Tr(A B), where A and B are symmetric
El::BigFloat frobenius_product_symmetric(const Block_Diagonal_Matrix &A,
                                         const Block_Diagonal_Matrix &B)
{
  El::BigFloat local_sum(0);
  for(size_t b = 0; b < A.blocks.size(); b++)
    {
      local_sum += El::Dotu(A.blocks[b], B.blocks[b]);
    }
  if(!A.blocks.empty() && A.blocks.front().Grid().Rank() != 0)
    {
      local_sum = 0;
    }
  return El::mpi::AllReduce(local_sum, El::mpi::COMM_WORLD);
}
