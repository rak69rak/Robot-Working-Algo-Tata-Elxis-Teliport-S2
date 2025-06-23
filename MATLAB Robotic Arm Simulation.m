function fiveDOFRobot_FinalGUI()

    % Updated Link lengths
    L1 = 1000; L2 = 240; L3 = 274; L4 = 70;

    % Create GUI window
    f = figure('Name', '5-DOF Robotic Arm with IK and Table', ...
               'NumberTitle', 'off', 'Position', [200 500 1200 600]);

    % Axes for 3D plot
    ax = axes('Parent', f, 'Position', [0.25 0.1 0.7 0.85]);
    view(135, 30); axis equal; grid on;
    xlabel('X'); ylabel('Y'); zlabel('Z');
    xlim([-300 300]); ylim([-300 300]); zlim([0 1600]);

    % Sliders for joints
    slider_info = {
        'Prismatic X (mm)',     0, 1200, 500;
        'Base Rotation (°)',   -180, 180, 0;
        'Shoulder Angle (°)',   -90, 90, 30;
        'Elbow 1 Angle (°)',    -90, 90, 45;
        'Elbow 2 Angle (°)',    -90, 90, 30
    };

    sliders = gobjects(5,1);
    for i = 1:5
        uicontrol('Style', 'text', 'Position', [30 330-60*i 150 20], ...
                  'String', slider_info{i,1}, 'FontWeight', 'bold');
        sliders(i) = uicontrol('Style', 'slider', ...
                   'Min', slider_info{i,2}, 'Max', slider_info{i,3}, ...
                   'Value', slider_info{i,4}, ...
                   'Position', [30 310-60*i 250 20], ...
                   'Callback', @(src,~) updatePlot());
    end

    % Inverse Kinematics Inputs (right-middle)
    uicontrol('Style', 'text', 'Position', [950 340 150 20], ...
              'String', 'Target X (mm)', 'FontWeight', 'bold');
    xInput = uicontrol('Style', 'edit', 'Position', [1100 340 80 25]);

    uicontrol('Style', 'text', 'Position', [950 300 150 20], ...
              'String', 'Target Y (mm)', 'FontWeight', 'bold');
    yInput = uicontrol('Style', 'edit', 'Position', [1100 300 80 25]);

    uicontrol('Style', 'text', 'Position', [950 260 150 20], ...
              'String', 'Target Z (mm)', 'FontWeight', 'bold');
    zInput = uicontrol('Style', 'edit', 'Position', [1100 260 80 25]);

    uicontrol('Style', 'pushbutton', 'String', 'Go to XYZ', ...
              'Position', [1020 220 100 25], 'Callback', @goToXYZ);

    % Result table
    resultTable = uitable(f, 'Data', cell(6,3), ...
        'ColumnName', {'Parameter', 'Value', 'Units'}, ...
        'ColumnWidth', {200, 100, 50}, ...
        'RowName', [], ...
        'FontSize', 11, ...
        'Position', [30 400 370 180]);

    % End effector position label
    endEffectorLabel = uicontrol('Style', 'text', ...
        'Position', [950 40 220 30], ...
        'FontSize', 12, 'FontWeight', 'bold', ...
        'String', 'End Effector: [X, Y, Z]');

    updatePlot();

    function updatePlot()
        q1 = sliders(1).Value;
        q2 = deg2rad(sliders(2).Value);
        q3 = deg2rad(sliders(3).Value);
        q4 = deg2rad(sliders(4).Value);
        q5 = deg2rad(sliders(5).Value);

        T1 = transl(q1, 0, 0);            
        T2 = trotz(q2);                    
        T3 = troty(q3) * transl(0, 0, L1); 
        T4 = troty(q4) * transl(0, 0, L2); 
        T5 = troty(q5) * transl(0, 0, L3); 
        T6 = transl(0, 0, L4);             % Camera link

        T01 = T1 * T2;
        T02 = T01 * T3;
        T03 = T02 * T4;
        T04 = T03 * T5;
        T05 = T04 * T6;

        p0 = [0 0 0 1]';
        p1 = T01 * p0;
        p2 = T02 * p0;
        p3 = T03 * p0;
        p4 = T04 * p0;
        p5 = T05 * p0;

        cla(ax); hold(ax, 'on');
        line(ax, [0 150], [0 0], [0 0], 'Color', [0.5 0.5 0.5], 'LineStyle', '--');

        h1 = plot3(ax, [p0(1) p1(1)], [p0(2) p1(2)], [p0(3) p1(3)], '-o', ...
            'Color', [0 0 0.8], 'LineWidth', 3, 'MarkerSize', 6);
        text(mean([p0(1) p1(1)]), mean([p0(2) p1(2)]), mean([p0(3) p1(3)]), 'Link 1');

        h2 = plot3(ax, [p1(1) p2(1)], [p1(2) p2(2)], [p1(3) p2(3)], '-o', ...
            'Color', [0.6 0 0.6], 'LineWidth', 3, 'MarkerSize', 6);
        text(mean([p1(1) p2(1)]), mean([p1(2) p2(2)]), mean([p1(3) p2(3)]), 'Link 2');

        h3 = plot3(ax, [p2(1) p3(1)], [p2(2) p3(2)], [p2(3) p3(3)], '-o', ...
            'Color', [0 0.6 0.6], 'LineWidth', 3, 'MarkerSize', 6);
        text(mean([p2(1) p3(1)]), mean([p2(2) p3(2)]), mean([p2(3) p3(3)]), 'Link 3');

        h4 = plot3(ax, [p3(1) p4(1)], [p3(2) p4(2)], [p3(3) p4(3)], '-o', ...
            'Color', [0 1 0], 'LineWidth', 3, 'MarkerSize', 6);
        text(mean([p3(1) p4(1)]), mean([p3(2) p4(2)]), mean([p3(3) p4(3)]), 'Camera Link');

        scatter3(ax, p1(1), p1(2), p1(3), 70, 'filled', 'MarkerFaceColor', [1 0 0]);
        scatter3(ax, p2(1), p2(2), p2(3), 70, 'filled', 'MarkerFaceColor', [1 0.5 0]);
        scatter3(ax, p3(1), p3(2), p3(3), 70, 'filled', 'MarkerFaceColor', [1 0 1]);
        scatter3(ax, p4(1), p4(2), p4(3), 90, 'filled', 'MarkerFaceColor', [0 1 0]);
        scatter3(ax, p5(1), p5(2), p5(3), 90, 'filled', 'MarkerFaceColor', [0 0 1]);

        frames = {T01, T02, T03, T04}; colors = eye(3); len = 50;
        for i = 1:4
            T = frames{i}; origin = T(1:3,4); R = T(1:3,1:3);
            for j = 1:3
                d = R(:,j) * len;
                quiver3(ax, origin(1), origin(2), origin(3), ...
                        d(1), d(2), d(3), 'Color', colors(j,:), 'LineWidth', 2);
            end
        end

        legend([h1 h2 h3 h4], {'Link 1','Link 2','Link 3','Camera Link'}, ...
               'Location', 'northeastoutside');
        title(ax, '5-DOF Robotic Arm with Inverse Kinematics and Table');

        resultTable.Data = {
            'Link 1 (Prismatic X)',  q1, 'mm';
            'Joint 1 (Base Z Rot)',  rad2deg(q2), 'deg';
            'Joint 2 (Shoulder Y)',  rad2deg(q3), 'deg';
            'Joint 3 (Elbow 1 Y)',   rad2deg(q4), 'deg';
            'Joint 4 (Elbow 2 Y)',   rad2deg(q5), 'deg';
            'End Effector (X,Y,Z)',  sprintf('[%.1f, %.1f, %.1f]', p5(1), p5(2), p5(3)), 'mm';
        };

        endEffectorLabel.String = sprintf('End Effector: [%.1f, %.1f, %.1f]', p5(1), p5(2), p5(3));

        hold(ax, 'off');
    end

    function goToXYZ(~,~)
        x = str2double(xInput.String);
        y = str2double(yInput.String);
        z = str2double(zInput.String);

        d = sqrt(x^2 + y^2);
        zOffset = z - L1;
        r = sqrt(d^2 + zOffset^2);

        if r > (L2 + L3)
            errordlg('Target is out of reach!', 'IK Error');
            return;
        end

        cosAngle = (r^2 - L2^2 - L3^2) / (2 * L2 * L3);
        theta3 = -acos(cosAngle);  % Invert elbow direction
        alpha = atan2(zOffset, d);
        beta = acos((L2^2 + r^2 - L3^2) / (2 * L2 * r));
        theta2 = alpha - beta;  % Adjust shoulder accordingly
        theta1 = atan2(y, x);

        targetVals = [d; rad2deg(theta1); rad2deg(theta2); rad2deg(theta3); 0];
        currentVals = cellfun(@(s) s.Value, num2cell(sliders));

        steps = 50;
        for i = 1:steps
            newVals = currentVals + (targetVals - currentVals) * (i/steps);
            for j = 1:5
                sliders(j).Value = newVals(j);
            end
            updatePlot();
            pause(0.01);
        end
    end
end

% Utility Transform Functions
function T = transl(x, y, z)
    T = eye(4); T(1:3,4) = [x; y; z];
end

function R = troty(theta)
    R = [cos(theta) 0 sin(theta) 0;
         0 1 0 0;
        -sin(theta) 0 cos(theta) 0;
         0 0 0 1];
end

function R = trotz(theta)
    R = [cos(theta) -sin(theta) 0 0;
         sin(theta)  cos(theta) 0 0;
         0 0 1 0;
         0 0 0 1];
end
