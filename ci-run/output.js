
$( window ).load(function() {
	$('.tasks .task-meta').on('click', function() {
		var taskOutput = $(this).next();
		if(taskOutput.hasClass('selected'))
		{
			taskOutput.removeClass('selected');
			$(this).removeClass('selected');
			$('body').removeClass('blur');
		}
		else
		{
			$('.tasks .task-output, .tasks .task-meta').removeClass('selected');
			taskOutput.addClass('selected');
			$(this).addClass('selected');
			$('body').addClass('blur');
		}
	});
});
